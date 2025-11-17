package fdfs

import (
	"bytes"
	"context"
	"fmt"
	"time"
)

// uploadFileWithRetry uploads a file with retry logic
func (c *Client) uploadFileWithRetry(ctx context.Context, localFilename string, metadata map[string]string, isAppender bool) (string, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		fileID, err := c.uploadFileInternal(ctx, localFilename, metadata, isAppender)
		if err == nil {
			return fileID, nil
		}
		lastErr = err

		// Don't retry on certain errors
		if err == ErrInvalidArgument || err == ErrFileNotFound {
			return "", err
		}

		// Wait before retry
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

// uploadFileInternal performs the actual file upload
func (c *Client) uploadFileInternal(ctx context.Context, localFilename string, metadata map[string]string, isAppender bool) (string, error) {
	// Read file content
	fileData, err := readFileContent(localFilename)
	if err != nil {
		return "", fmt.Errorf("failed to read file: %w", err)
	}

	// Get file extension
	extName := getFileExtName(localFilename)

	return c.uploadBufferInternal(ctx, fileData, extName, metadata, isAppender)
}

// uploadBufferWithRetry uploads buffer with retry logic
func (c *Client) uploadBufferWithRetry(ctx context.Context, data []byte, fileExtName string, metadata map[string]string, isAppender bool) (string, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		fileID, err := c.uploadBufferInternal(ctx, data, fileExtName, metadata, isAppender)
		if err == nil {
			return fileID, nil
		}
		lastErr = err

		if err == ErrInvalidArgument {
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

// uploadBufferInternal performs the actual buffer upload
func (c *Client) uploadBufferInternal(ctx context.Context, data []byte, fileExtName string, metadata map[string]string, isAppender bool) (string, error) {
	// Get storage server from tracker
	storageServer, err := c.getStorageServer(ctx, "")
	if err != nil {
		return "", err
	}

	// Get connection to storage server
	conn, err := c.storagePool.Get(ctx, storageServer.IPAddr+":"+fmt.Sprintf("%d", storageServer.Port))
	if err != nil {
		return "", err
	}
	defer c.storagePool.Put(conn)

	// Prepare upload command
	cmd := byte(StorageProtoCmdUploadFile)
	if isAppender {
		cmd = byte(StorageProtoCmdUploadAppenderFile)
	}

	// Build request
	extNameBytes := padString(fileExtName, FdfsFileExtNameMaxLen)
	storePathIndex := byte(storageServer.StorePathIndex)

	bodyLen := 1 + FdfsFileExtNameMaxLen + int64(len(data))
	reqHeader := encodeHeader(bodyLen, cmd, 0)

	// Send header
	if err := conn.Send(reqHeader, c.config.NetworkTimeout); err != nil {
		return "", err
	}

	// Send store path index
	if err := conn.Send([]byte{storePathIndex}, c.config.NetworkTimeout); err != nil {
		return "", err
	}

	// Send file extension
	if err := conn.Send(extNameBytes, c.config.NetworkTimeout); err != nil {
		return "", err
	}

	// Send file data
	if err := conn.Send(data, c.config.NetworkTimeout); err != nil {
		return "", err
	}

	// Receive response header
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

	// Receive response body
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

	groupName := unpadString(respBody[:FdfsGroupNameMaxLen])
	remoteFilename := string(respBody[FdfsGroupNameMaxLen:])

	fileID := joinFileID(groupName, remoteFilename)

	// Set metadata if provided
	if len(metadata) > 0 {
		if err := c.setMetadataInternal(ctx, fileID, metadata, MetadataOverwrite); err != nil {
			// Metadata setting failed, but file is uploaded
			// Log the error but don't fail the upload
			return fileID, nil
		}
	}

	return fileID, nil
}

// getStorageServer gets a storage server from tracker
func (c *Client) getStorageServer(ctx context.Context, groupName string) (*StorageServer, error) {
	conn, err := c.trackerPool.Get(ctx, "")
	if err != nil {
		return nil, err
	}
	defer c.trackerPool.Put(conn)

	// Prepare request
	var bodyLen int64
	var cmd byte

	if groupName == "" {
		cmd = TrackerProtoCmdServiceQueryStoreWithoutGroupOne
		bodyLen = 0
	} else {
		cmd = TrackerProtoCmdServiceQueryStoreWithGroupOne
		bodyLen = FdfsGroupNameMaxLen
	}

	header := encodeHeader(bodyLen, cmd, 0)

	// Send header
	if err := conn.Send(header, c.config.NetworkTimeout); err != nil {
		return nil, err
	}

	// Send group name if specified
	if groupName != "" {
		groupNameBytes := padString(groupName, FdfsGroupNameMaxLen)
		if err := conn.Send(groupNameBytes, c.config.NetworkTimeout); err != nil {
			return nil, err
		}
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

	if respHeaderParsed.Length <= 0 {
		return nil, ErrNoStorageServer
	}

	respBody, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	// Parse storage server info
	if len(respBody) < FdfsGroupNameMaxLen+IPAddressSize+9 {
		return nil, ErrInvalidResponse
	}

	offset := FdfsGroupNameMaxLen
	ipAddr := unpadString(respBody[offset : offset+IPAddressSize])
	offset += IPAddressSize

	port := int(decodeInt64(respBody[offset : offset+8]))
	offset += 8

	storePathIndex := respBody[offset]

	return &StorageServer{
		IPAddr:         ipAddr,
		Port:           port,
		StorePathIndex: storePathIndex,
	}, nil
}

// downloadFileWithRetry downloads a file with retry logic
func (c *Client) downloadFileWithRetry(ctx context.Context, fileID string, offset, length int64) ([]byte, error) {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		data, err := c.downloadFileInternal(ctx, fileID, offset, length)
		if err == nil {
			return data, nil
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

// downloadFileInternal performs the actual file download
func (c *Client) downloadFileInternal(ctx context.Context, fileID string, offset, length int64) ([]byte, error) {
	groupName, remoteFilename, err := splitFileID(fileID)
	if err != nil {
		return nil, err
	}

	// Get storage server for download
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
	bodyLen := int64(16 + len(remoteFilename))
	header := encodeHeader(bodyLen, StorageProtoCmdDownloadFile, 0)

	var buf bytes.Buffer
	buf.Write(encodeInt64(offset))
	buf.Write(encodeInt64(length))
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

	if respHeaderParsed.Length <= 0 {
		return []byte{}, nil
	}

	// Receive file data
	data, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	return data, nil
}

// getDownloadStorageServer gets a storage server for downloading
func (c *Client) getDownloadStorageServer(ctx context.Context, groupName, remoteFilename string) (*StorageServer, error) {
	conn, err := c.trackerPool.Get(ctx, "")
	if err != nil {
		return nil, err
	}
	defer c.trackerPool.Put(conn)

	// Build request
	bodyLen := int64(FdfsGroupNameMaxLen + len(remoteFilename))
	header := encodeHeader(bodyLen, TrackerProtoCmdServiceQueryFetchOne, 0)

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

	respBody, err := conn.ReceiveFull(int(respHeaderParsed.Length), c.config.NetworkTimeout)
	if err != nil {
		return nil, err
	}

	// Parse response
	if len(respBody) < FdfsGroupNameMaxLen+IPAddressSize+8 {
		return nil, ErrInvalidResponse
	}

	offset := FdfsGroupNameMaxLen
	ipAddr := unpadString(respBody[offset : offset+IPAddressSize])
	offset += IPAddressSize

	port := int(decodeInt64(respBody[offset : offset+8]))

	return &StorageServer{
		IPAddr: ipAddr,
		Port:   port,
	}, nil
}

// downloadToFileWithRetry downloads to file with retry
func (c *Client) downloadToFileWithRetry(ctx context.Context, fileID, localFilename string) error {
	data, err := c.downloadFileWithRetry(ctx, fileID, 0, 0)
	if err != nil {
		return err
	}

	return writeFileContent(localFilename, data)
}

// deleteFileWithRetry deletes a file with retry
func (c *Client) deleteFileWithRetry(ctx context.Context, fileID string) error {
	var lastErr error
	for i := 0; i < c.config.RetryCount; i++ {
		err := c.deleteFileInternal(ctx, fileID)
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

// deleteFileInternal performs the actual file deletion
func (c *Client) deleteFileInternal(ctx context.Context, fileID string) error {
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
	bodyLen := int64(FdfsGroupNameMaxLen + len(remoteFilename))
	header := encodeHeader(bodyLen, StorageProtoCmdDeleteFile, 0)

	var buf bytes.Buffer
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
