package fdfs

import (
	"bytes"
	"context"
	"fmt"
	"time"
)

// setMetadataWithRetry sets metadata with retry logic
func (c *Client) setMetadataWithRetry(ctx context.Context, fileID string, metadata map[string]string, flag MetadataFlag) error {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		err := c.setMetadataInternal(ctx, fileID, metadata, flag)
		if err == nil {
			return nil
		}
		lastErr = err

		if err == ErrFileNotFound || err == ErrInvalidFileID {
			return err
		}

		if i < c.config.RetryCount-1 {
			select {
			case <-ctx.Done():
				return ctx.Err()
			case <-time.After(time.Second * time.Duration(i+1)):
			}
		}
	}
	return lastErr
}

// setMetadataInternal performs the actual metadata setting
func (c *Client) setMetadataInternal(ctx context.Context, fileID string, metadata map[string]string, flag MetadataFlag) error {
	groupName, remoteFilename, err := splitFileID(fileID)
	if err != nil {
		return err
	}

	// Get storage server
	storageServer, err := c.getDownloadStorageServer(ctx, groupName, remoteFilename)
	if err != nil {
		return err
	}

	// Get connection
	conn, err := c.storagePool.Get(ctx, storageServer.IPAddr+":"+fmt.Sprintf("%d", storageServer.Port))
	if err != nil {
		return err
	}
	defer c.storagePool.Put(conn)

	// Encode metadata
	metaBytes := encodeMetadata(metadata)

	// Build request
	bodyLen := int64(2*8 + 1 + FdfsGroupNameMaxLen + len(remoteFilename) + len(metaBytes))
	header := encodeHeader(bodyLen, StorageProtoCmdSetMetadata, 0)

	var buf bytes.Buffer
	buf.Write(encodeInt64(int64(len(remoteFilename))))
	buf.Write(encodeInt64(int64(len(metaBytes))))
	buf.WriteByte(byte(flag))
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))
	buf.Write(metaBytes)

	// Send request
	if err := conn.Send(header, c.config.NetworkTimeout); err != nil {
		return err
	}
	if err := conn.Send(buf.Bytes(), c.config.NetworkTimeout); err != nil {
		return err
	}

	// Receive response
	respHeader, err := conn.ReceiveFull(FdfsProtoHeaderLen, c.config.NetworkTimeout)
	if err != nil {
		return err
	}

	respHeaderParsed, err := decodeHeader(respHeader)
	if err != nil {
		return err
	}

	if respHeaderParsed.Status != 0 {
		return mapStatusToError(respHeaderParsed.Status)
	}

	return nil
}

// getMetadataWithRetry gets metadata with retry logic
func (c *Client) getMetadataWithRetry(ctx context.Context, fileID string) (map[string]string, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		metadata, err := c.getMetadataInternal(ctx, fileID)
		if err == nil {
			return metadata, nil
		}
		lastErr = err

		if err == ErrFileNotFound || err == ErrInvalidFileID {
			return nil, err
		}

		if i < c.config.RetryCount-1 {
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			case <-time.After(time.Second * time.Duration(i+1)):
			}
		}
	}
	return nil, lastErr
}

// getMetadataInternal performs the actual metadata retrieval
func (c *Client) getMetadataInternal(ctx context.Context, fileID string) (map[string]string, error) {
	groupName, remoteFilename, err := splitFileID(fileID)
	if err != nil {
		return nil, err
	}

	// Get storage server
	storageServer, err := c.getDownloadStorageServer(ctx, groupName, remoteFilename)
	if err != nil {
		return nil, err
	}

	// Get connection
	conn, err := c.storagePool.Get(ctx, storageServer.IPAddr+":"+fmt.Sprintf("%d", storageServer.Port))
	if err != nil {
		return nil, err
	}
	defer c.storagePool.Put(conn)

	// Build request
	bodyLen := int64(FdfsGroupNameMaxLen + len(remoteFilename))
	header := encodeHeader(bodyLen, StorageProtoCmdGetMetadata, 0)

	var buf bytes.Buffer
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))

	// Send request
	if err := conn.Send(header, c.config.NetworkTimeout); err != nil {
		return nil, err
	}
	if err := conn.Send(buf.Bytes(), c.config.NetworkTimeout); err != nil {
		return nil, err
	}

	// Receive response
	respHeader, err := conn.ReceiveFull(FdfsProtoHeaderLen, c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	respHeaderParsed, err := decodeHeader(respHeader)
	if err != nil {
		return nil, err
	}

	if respHeaderParsed.Status != 0 {
		return nil, mapStatusToError(respHeaderParsed.Status)
	}

	if respHeaderParsed.Length == 0 {
		return make(map[string]string), nil
	}

	// Receive metadata
	metaBytes, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	// Decode metadata
	return decodeMetadata(metaBytes)
}

// getFileInfoWithRetry gets file info with retry logic
func (c *Client) getFileInfoWithRetry(ctx context.Context, fileID string) (*FileInfo, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		info, err := c.getFileInfoInternal(ctx, fileID)
		if err == nil {
			return info, nil
		}
		lastErr = err

		if err == ErrFileNotFound || err == ErrInvalidFileID {
			return nil, err
		}

		if i < c.config.RetryCount-1 {
			select {
			case <-ctx.Done():
				return nil, ctx.Err()
			case <-time.After(time.Second * time.Duration(i+1)):
			}
		}
	}
	return nil, lastErr
}

// getFileInfoInternal performs the actual file info retrieval
func (c *Client) getFileInfoInternal(ctx context.Context, fileID string) (*FileInfo, error) {
	groupName, remoteFilename, err := splitFileID(fileID)
	if err != nil {
		return nil, err
	}

	// Get storage server
	storageServer, err := c.getDownloadStorageServer(ctx, groupName, remoteFilename)
	if err != nil {
		return nil, err
	}

	// Get connection
	conn, err := c.storagePool.Get(ctx, storageServer.IPAddr+":"+fmt.Sprintf("%d", storageServer.Port))
	if err != nil {
		return nil, err
	}
	defer c.storagePool.Put(conn)

	// Build request
	bodyLen := int64(FdfsGroupNameMaxLen + len(remoteFilename))
	header := encodeHeader(bodyLen, StorageProtoCmdQueryFileInfo, 0)

	var buf bytes.Buffer
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))

	// Send request
	if err := conn.Send(header, c.config.NetworkTimeout); err != nil {
		return nil, err
	}
	if err := conn.Send(buf.Bytes(), c.config.NetworkTimeout); err != nil {
		return nil, err
	}

	// Receive response
	respHeader, err := conn.ReceiveFull(FdfsProtoHeaderLen, c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	respHeaderParsed, err := decodeHeader(respHeader)
	if err != nil {
		return nil, err
	}

	if respHeaderParsed.Status != 0 {
		return nil, mapStatusToError(respHeaderParsed.Status)
	}

	// Expected response: file_size(8) + create_timestamp(8) + crc32(4) + ip_addr(16)
	expectedLen := 8 + 8 + 4 + IPAddressSize
	if respHeaderParsed.Length < int64(expectedLen) {
		return nil, ErrInvalidResponse
	}

	respBody, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	// Parse file info
	fileSize := decodeInt64(respBody[0:8])
	createTimestamp := decodeInt64(respBody[8:16])
	crc32 := uint32(decodeInt32(respBody[16:20]))
	ipAddr := unpadString(respBody[20:36])

	return &FileInfo{
		FileSize:     fileSize,
		CreateTime:   time.Unix(createTimestamp, 0),
		CRC32:        crc32,
		SourceIPAddr: ipAddr,
	}, nil
}
