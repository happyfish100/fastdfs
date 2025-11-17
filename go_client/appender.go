package fdfs

import (
	"bytes"
	"context"
	"fmt"
	"time"
)

// uploadSlaveFileWithRetry uploads a slave file with retry logic
func (c *Client) uploadSlaveFileWithRetry(ctx context.Context, masterFileID, prefixName, fileExtName string, data []byte, metadata map[string]string) (string, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		fileID, err := c.uploadSlaveFileInternal(ctx, masterFileID, prefixName, fileExtName, data, metadata)
		if err == nil {
			return fileID, nil
		}
		lastErr = err

		if err == ErrInvalidFileID || err == ErrFileNotFound {
			return "", err
		}

		if i < c.config.RetryCount-1 {
			select {
			case <-ctx.Done():
				return "", ctx.Err()
			case <-time.After(time.Second * time.Duration(i+1)):
			}
		}
	}
	return "", lastErr
}

// uploadSlaveFileInternal performs the actual slave file upload
func (c *Client) uploadSlaveFileInternal(ctx context.Context, masterFileID, prefixName, fileExtName string, data []byte, metadata map[string]string) (string, error) {
	groupName, masterFilename, err := splitFileID(masterFileID)
	if err != nil {
		return "", err
	}

	// Validate prefix name
	if len(prefixName) > FdfsFilePrefixMaxLen {
		prefixName = prefixName[:FdfsFilePrefixMaxLen]
	}

	// Get storage server
	storageServer, err := c.getDownloadStorageServer(ctx, groupName, masterFilename)
	if err != nil {
		return "", err
	}

	// Get connection
	conn, err := c.storagePool.Get(ctx, storageServer.IPAddr+":"+fmt.Sprintf("%d", storageServer.Port))
	if err != nil {
		return "", err
	}
	defer c.storagePool.Put(conn)

	// Build request
	extNameBytes := padString(fileExtName, FdfsFileExtNameMaxLen)
	prefixNameBytes := padString(prefixName, FdfsFilePrefixMaxLen)

	bodyLen := int64(len(masterFilename) + FdfsFilePrefixMaxLen + FdfsFileExtNameMaxLen + 8 + len(data))
	header := encodeHeader(bodyLen, StorageProtoCmdUploadSlaveFile, 0)

	var buf bytes.Buffer
	buf.Write(encodeInt64(int64(len(masterFilename))))
	buf.Write(encodeInt64(int64(len(data))))
	buf.Write(prefixNameBytes)
	buf.Write(extNameBytes)
	buf.Write([]byte(masterFilename))
	buf.Write(data)

	// Send request
	if err := conn.Send(header, c.config.NetworkTimeout); err != nil {
		return "", err
	}
	if err := conn.Send(buf.Bytes(), c.config.NetworkTimeout); err != nil {
		return "", err
	}

	// Receive response
	respHeader, err := conn.ReceiveFull(FdfsProtoHeaderLen, c.config.NetworkTimeout)
	if err != nil {
		return "", err
	}

	respHeaderParsed, err := decodeHeader(respHeader)
	if err != nil {
		return "", err
	}

	if respHeaderParsed.Status != 0 {
		return "", mapStatusToError(respHeaderParsed.Status)
	}

	if respHeaderParsed.Length <= 0 {
		return "", ErrInvalidResponse
	}

	respBody, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return "", err
	}

	// Parse response
	if len(respBody) < FdfsGroupNameMaxLen {
		return "", ErrInvalidResponse
	}

	respGroupName := unpadString(respBody[:FdfsGroupNameMaxLen])
	remoteFilename := string(respBody[FdfsGroupNameMaxLen:])

	fileID := joinFileID(respGroupName, remoteFilename)

	// Set metadata if provided
	if len(metadata) > 0 {
		c.setMetadataInternal(ctx, fileID, metadata, MetadataOverwrite)
	}

	return fileID, nil
}

// appendFileWithRetry appends data to a file with retry logic
func (c *Client) appendFileWithRetry(ctx context.Context, fileID string, data []byte) error {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		err := c.appendFileInternal(ctx, fileID, data)
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

// appendFileInternal performs the actual file append
func (c *Client) appendFileInternal(ctx context.Context, fileID string, data []byte) error {
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

	// Build request
	bodyLen := int64(FdfsGroupNameMaxLen + len(remoteFilename) + len(data))
	header := encodeHeader(bodyLen, StorageProtoCmdAppendFile, 0)

	var buf bytes.Buffer
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))
	buf.Write(data)

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

// modifyFileWithRetry modifies a file with retry logic
func (c *Client) modifyFileWithRetry(ctx context.Context, fileID string, offset int64, data []byte) error {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		err := c.modifyFileInternal(ctx, fileID, offset, data)
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

// modifyFileInternal performs the actual file modification
func (c *Client) modifyFileInternal(ctx context.Context, fileID string, offset int64, data []byte) error {
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

	// Build request
	bodyLen := int64(16 + FdfsGroupNameMaxLen + len(remoteFilename) + len(data))
	header := encodeHeader(bodyLen, StorageProtoCmdModifyFile, 0)

	var buf bytes.Buffer
	buf.Write(encodeInt64(offset))
	buf.Write(encodeInt64(int64(len(data))))
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))
	buf.Write(data)

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

// truncateFileWithRetry truncates a file with retry logic
func (c *Client) truncateFileWithRetry(ctx context.Context, fileID string, size int64) error {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		err := c.truncateFileInternal(ctx, fileID, size)
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

// truncateFileInternal performs the actual file truncation
func (c *Client) truncateFileInternal(ctx context.Context, fileID string, size int64) error {
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

	// Build request
	bodyLen := int64(16 + FdfsGroupNameMaxLen + len(remoteFilename))
	header := encodeHeader(bodyLen, StorageProtoCmdTruncateFile, 0)

	var buf bytes.Buffer
	buf.Write(encodeInt64(int64(len(remoteFilename))))
	buf.Write(encodeInt64(size))
	buf.Write(padString(groupName, FdfsGroupNameMaxLen))
	buf.Write([]byte(remoteFilename))

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
