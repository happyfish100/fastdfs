package fdfs

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
)

// encodeHeader encodes a FastDFS protocol header into a 10-byte array.
// The header format is:
//   - Bytes 0-7: Body length (8 bytes, big-endian uint64)
//   - Byte 8: Command code
//   - Byte 9: Status code (0 for request, error code for response)
//
// Parameters:
//   - length: the length of the message body in bytes
//   - cmd: the protocol command code
//   - status: the status code (typically 0 for requests)
//
// Returns a 10-byte header ready to be sent to the server.
func encodeHeader(length int64, cmd byte, status byte) []byte {
	header := make([]byte, FdfsProtoHeaderLen)
	binary.BigEndian.PutUint64(header[0:8], uint64(length))
	header[8] = cmd
	header[9] = status
	return header
}

// decodeHeader decodes a FastDFS protocol header from a byte array.
// The header must be exactly 10 bytes long.
//
// Parameters:
//   - data: the raw header bytes (must be at least 10 bytes)
//
// Returns:
//   - TrackerHeader: parsed header with length, command, and status
//   - error: ErrInvalidResponse if data is too short
func decodeHeader(data []byte) (*TrackerHeader, error) {
	if len(data) < FdfsProtoHeaderLen {
		return nil, ErrInvalidResponse
	}

	header := &TrackerHeader{
		Length: int64(binary.BigEndian.Uint64(data[0:8])),
		Cmd:    data[8],
		Status: data[9],
	}

	return header, nil
}

// splitFileID splits a FastDFS file ID into its components.
// A file ID has the format: "groupName/path/to/file"
// For example: "group1/M00/00/00/wKgBcFxyz.jpg"
//
// Parameters:
//   - fileID: the complete file ID string
//
// Returns:
//   - groupName: the storage group name (max 16 chars)
//   - remoteFilename: the path and filename on the storage server
//   - error: ErrInvalidFileID if the format is invalid
func splitFileID(fileID string) (string, string, error) {
	if fileID == "" {
		return "", "", ErrInvalidFileID
	}

	parts := strings.SplitN(fileID, "/", 2)
	if len(parts) != 2 {
		return "", "", ErrInvalidFileID
	}

	groupName := parts[0]
	remoteFilename := parts[1]

	if len(groupName) == 0 || len(groupName) > FdfsGroupNameMaxLen {
		return "", "", ErrInvalidFileID
	}

	if len(remoteFilename) == 0 {
		return "", "", ErrInvalidFileID
	}

	return groupName, remoteFilename, nil
}

// joinFileID constructs a complete file ID from its components.
// This is the inverse operation of splitFileID.
//
// Parameters:
//   - groupName: the storage group name
//   - remoteFilename: the path and filename on the storage server
//
// Returns a complete file ID in the format "groupName/remoteFilename"
func joinFileID(groupName, remoteFilename string) string {
	return groupName + "/" + remoteFilename
}

// encodeMetadata encodes metadata key-value pairs into FastDFS wire format.
// The format uses special separators:
//   - Field separator (0x02) between key and value
//   - Record separator (0x01) between different key-value pairs
//
// Format: key1<0x02>value1<0x01>key2<0x02>value2<0x01>
//
// Keys are truncated to 64 bytes and values to 256 bytes if they exceed limits.
//
// Parameters:
//   - metadata: map of key-value pairs to encode
//
// Returns the encoded byte array, or nil if metadata is empty.
func encodeMetadata(metadata map[string]string) []byte {
	if len(metadata) == 0 {
		return nil
	}

	var buf bytes.Buffer
	for key, value := range metadata {
		if len(key) > FdfsMaxMetaNameLen {
			key = key[:FdfsMaxMetaNameLen]
		}
		if len(value) > FdfsMaxMetaValueLen {
			value = value[:FdfsMaxMetaValueLen]
		}
		buf.WriteString(key)
		buf.WriteByte(FdfsFieldSeparator)
		buf.WriteString(value)
		buf.WriteByte(FdfsRecordSeparator)
	}

	return buf.Bytes()
}

// decodeMetadata decodes FastDFS wire format metadata into a map.
// This is the inverse operation of encodeMetadata.
//
// The function parses records separated by 0x01 and fields separated by 0x02.
// Invalid records (not exactly 2 fields) are silently skipped.
//
// Parameters:
//   - data: the raw metadata bytes from the server
//
// Returns:
//   - map[string]string: decoded key-value pairs
//   - error: always nil (for future compatibility)
func decodeMetadata(data []byte) (map[string]string, error) {
	if len(data) == 0 {
		return make(map[string]string), nil
	}

	metadata := make(map[string]string)
	records := bytes.Split(data, []byte{FdfsRecordSeparator})

	for _, record := range records {
		if len(record) == 0 {
			continue
		}

		fields := bytes.Split(record, []byte{FdfsFieldSeparator})
		if len(fields) != 2 {
			continue
		}

		key := string(fields[0])
		value := string(fields[1])
		metadata[key] = value
	}

	return metadata, nil
}

// getFileExtName extracts and validates the file extension from a filename.
// The extension is extracted without the leading dot and truncated to 6 characters
// if it exceeds the FastDFS maximum.
//
// Examples:
//   - "test.jpg" -> "jpg"
//   - "file.tar.gz" -> "gz"
//   - "noext" -> ""
//   - "file.verylongext" -> "veryl" (truncated)
//
// Parameters:
//   - filename: the filename to extract extension from
//
// Returns the file extension without the dot, truncated to 6 chars max.
func getFileExtName(filename string) string {
	ext := filepath.Ext(filename)
	if len(ext) > 0 && ext[0] == '.' {
		ext = ext[1:]
	}
	if len(ext) > FdfsFileExtNameMaxLen {
		ext = ext[:FdfsFileExtNameMaxLen]
	}
	return ext
}

// readFileContent reads the entire contents of a file from the filesystem.
// The file is read into memory, so this should not be used for very large files.
//
// Parameters:
//   - filename: path to the file to read
//
// Returns:
//   - []byte: the complete file contents
//   - error: if the file cannot be opened, stat'd, or read
func readFileContent(filename string) ([]byte, error) {
	file, err := os.Open(filename)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer file.Close()

	stat, err := file.Stat()
	if err != nil {
		return nil, fmt.Errorf("failed to stat file: %w", err)
	}

	if stat.Size() == 0 {
		return []byte{}, nil
	}

	data := make([]byte, stat.Size())
	_, err = io.ReadFull(file, data)
	if err != nil {
		return nil, fmt.Errorf("failed to read file: %w", err)
	}

	return data, nil
}

// writeFileContent writes data to a file, creating parent directories if needed.
// If the file already exists, it will be truncated.
//
// Parameters:
//   - filename: path where the file should be written
//   - data: the content to write
//
// Returns an error if directories cannot be created or file cannot be written.
func writeFileContent(filename string, data []byte) error {
	dir := filepath.Dir(filename)
	if err := os.MkdirAll(dir, 0755); err != nil {
		return fmt.Errorf("failed to create directory: %w", err)
	}

	file, err := os.Create(filename)
	if err != nil {
		return fmt.Errorf("failed to create file: %w", err)
	}
	defer file.Close()

	_, err = file.Write(data)
	if err != nil {
		return fmt.Errorf("failed to write file: %w", err)
	}

	return nil
}

// padString pads a string to a fixed length with null bytes (0x00).
// This is used to create fixed-width fields in the FastDFS protocol.
// If the string is longer than length, it will be truncated.
//
// Parameters:
//   - s: the string to pad
//   - length: the desired length in bytes
//
// Returns a byte array of exactly 'length' bytes.
func padString(s string, length int) []byte {
	buf := make([]byte, length)
	copy(buf, []byte(s))
	return buf
}

// unpadString removes trailing null bytes from a byte slice.
// This is the inverse of padString, used to extract strings from
// fixed-width protocol fields.
//
// Parameters:
//   - data: byte array with potential trailing nulls
//
// Returns the string with trailing null bytes removed.
func unpadString(data []byte) string {
	return string(bytes.TrimRight(data, "\x00"))
}

// encodeInt64 encodes a 64-bit integer to an 8-byte big-endian representation.
// FastDFS protocol uses big-endian byte order for all numeric fields.
//
// Parameters:
//   - n: the integer to encode
//
// Returns an 8-byte array in big-endian format.
func encodeInt64(n int64) []byte {
	buf := make([]byte, 8)
	binary.BigEndian.PutUint64(buf, uint64(n))
	return buf
}

// decodeInt64 decodes an 8-byte big-endian representation to a 64-bit integer.
// This is the inverse of encodeInt64.
//
// Parameters:
//   - data: byte array (must be at least 8 bytes)
//
// Returns the decoded integer, or 0 if data is too short.
func decodeInt64(data []byte) int64 {
	if len(data) < 8 {
		return 0
	}
	return int64(binary.BigEndian.Uint64(data))
}

// encodeInt32 encodes a 32-bit integer to a 4-byte big-endian representation.
//
// Parameters:
//   - n: the integer to encode
//
// Returns a 4-byte array in big-endian format.
func encodeInt32(n int32) []byte {
	buf := make([]byte, 4)
	binary.BigEndian.PutUint32(buf, uint32(n))
	return buf
}

// decodeInt32 decodes a 4-byte big-endian representation to a 32-bit integer.
//
// Parameters:
//   - data: byte array (must be at least 4 bytes)
//
// Returns the decoded integer, or 0 if data is too short.
func decodeInt32(data []byte) int32 {
	if len(data) < 4 {
		return 0
	}
	return int32(binary.BigEndian.Uint32(data))
}
