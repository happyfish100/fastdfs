// Package fdfs provides a Go client for FastDFS distributed file system.
//
// # Copyright (C) 2025 FastDFS Go Client Contributors
//
// FastDFS may be copied only under the terms of the GNU General
// Public License V3, which may be found in the FastDFS source kit.
package fdfs

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"
)

// Client represents a FastDFS client instance.
type Client struct {
	config      *ClientConfig
	trackerPool *ConnectionPool
	storagePool *ConnectionPool
	mu          sync.RWMutex
	closed      bool
}

// ClientConfig holds the configuration for FastDFS client.
type ClientConfig struct {
	// TrackerAddrs is the list of tracker server addresses in format "host:port"
	TrackerAddrs []string

	// MaxConns is the maximum number of connections per tracker server
	MaxConns int

	// ConnectTimeout is the timeout for establishing connections
	ConnectTimeout time.Duration

	// NetworkTimeout is the timeout for network I/O operations
	NetworkTimeout time.Duration

	// IdleTimeout is the timeout for idle connections in the pool
	IdleTimeout time.Duration

	// EnablePool enables connection pooling (default: true)
	EnablePool bool

	// RetryCount is the number of retries for failed operations
	RetryCount int
}

// NewClient creates a new FastDFS client with the given configuration.
func NewClient(config *ClientConfig) (*Client, error) {
	if err := validateConfig(config); err != nil {
		return nil, fmt.Errorf("invalid config: %w", err)
	}

	// Set defaults
	if config.MaxConns == 0 {
		config.MaxConns = 10
	}
	if config.ConnectTimeout == 0 {
		config.ConnectTimeout = 5 * time.Second
	}
	if config.NetworkTimeout == 0 {
		config.NetworkTimeout = 30 * time.Second
	}
	if config.IdleTimeout == 0 {
		config.IdleTimeout = 60 * time.Second
	}
	if config.RetryCount == 0 {
		config.RetryCount = 3
	}

	client := &Client{
		config: config,
	}

	// Initialize tracker connection pool
	trackerPool, err := NewConnectionPool(config.TrackerAddrs, config.MaxConns,
		config.ConnectTimeout, config.IdleTimeout)
	if err != nil {
		return nil, fmt.Errorf("failed to create tracker pool: %w", err)
	}
	client.trackerPool = trackerPool

	// Initialize storage connection pool
	storagePool, err := NewConnectionPool([]string{}, config.MaxConns,
		config.ConnectTimeout, config.IdleTimeout)
	if err != nil {
		trackerPool.Close()
		return nil, fmt.Errorf("failed to create storage pool: %w", err)
	}
	client.storagePool = storagePool

	return client, nil
}

// UploadFile uploads a file from the local filesystem to FastDFS.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - localFilename: path to the local file
//   - metadata: optional metadata key-value pairs
//
// Returns the file ID on success.
func (c *Client) UploadFile(ctx context.Context, localFilename string, metadata map[string]string) (string, error) {
	if err := c.checkClosed(); err != nil {
		return "", err
	}

	return c.uploadFileWithRetry(ctx, localFilename, metadata, false)
}

// UploadBuffer uploads data from a byte buffer to FastDFS.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - data: file content as byte slice
//   - fileExtName: file extension without dot (e.g., "jpg", "txt")
//   - metadata: optional metadata key-value pairs
//
// Returns the file ID on success.
func (c *Client) UploadBuffer(ctx context.Context, data []byte, fileExtName string, metadata map[string]string) (string, error) {
	if err := c.checkClosed(); err != nil {
		return "", err
	}

	return c.uploadBufferWithRetry(ctx, data, fileExtName, metadata, false)
}

// UploadAppenderFile uploads an appender file that can be modified later.
func (c *Client) UploadAppenderFile(ctx context.Context, localFilename string, metadata map[string]string) (string, error) {
	if err := c.checkClosed(); err != nil {
		return "", err
	}

	return c.uploadFileWithRetry(ctx, localFilename, metadata, true)
}

// UploadAppenderBuffer uploads an appender file from buffer.
func (c *Client) UploadAppenderBuffer(ctx context.Context, data []byte, fileExtName string, metadata map[string]string) (string, error) {
	if err := c.checkClosed(); err != nil {
		return "", err
	}

	return c.uploadBufferWithRetry(ctx, data, fileExtName, metadata, true)
}

// UploadSlaveFile uploads a slave file associated with a master file.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - masterFileID: the master file ID
//   - prefixName: prefix for the slave file (e.g., "thumb", "small")
//   - fileExtName: file extension without dot
//   - data: file content
//   - metadata: optional metadata
//
// Returns the slave file ID on success.
func (c *Client) UploadSlaveFile(ctx context.Context, masterFileID, prefixName, fileExtName string,
	data []byte, metadata map[string]string) (string, error) {
	if err := c.checkClosed(); err != nil {
		return "", err
	}

	return c.uploadSlaveFileWithRetry(ctx, masterFileID, prefixName, fileExtName, data, metadata)
}

// DownloadFile downloads a file from FastDFS and returns its content.
func (c *Client) DownloadFile(ctx context.Context, fileID string) ([]byte, error) {
	if err := c.checkClosed(); err != nil {
		return nil, err
	}

	return c.downloadFileWithRetry(ctx, fileID, 0, 0)
}

// DownloadFileRange downloads a specific range of bytes from a file.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - fileID: the file ID to download
//   - offset: starting byte offset
//   - length: number of bytes to download (0 means to end of file)
func (c *Client) DownloadFileRange(ctx context.Context, fileID string, offset, length int64) ([]byte, error) {
	if err := c.checkClosed(); err != nil {
		return nil, err
	}

	return c.downloadFileWithRetry(ctx, fileID, offset, length)
}

// DownloadToFile downloads a file and saves it to the local filesystem.
func (c *Client) DownloadToFile(ctx context.Context, fileID, localFilename string) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.downloadToFileWithRetry(ctx, fileID, localFilename)
}

// DeleteFile deletes a file from FastDFS.
func (c *Client) DeleteFile(ctx context.Context, fileID string) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.deleteFileWithRetry(ctx, fileID)
}

// AppendFile appends data to an appender file.
func (c *Client) AppendFile(ctx context.Context, fileID string, data []byte) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.appendFileWithRetry(ctx, fileID, data)
}

// ModifyFile modifies content of an appender file at specified offset.
func (c *Client) ModifyFile(ctx context.Context, fileID string, offset int64, data []byte) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.modifyFileWithRetry(ctx, fileID, offset, data)
}

// TruncateFile truncates an appender file to specified size.
func (c *Client) TruncateFile(ctx context.Context, fileID string, size int64) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.truncateFileWithRetry(ctx, fileID, size)
}

// SetMetadata sets metadata for a file.
//
// Parameters:
//   - ctx: context for cancellation and timeout
//   - fileID: the file ID
//   - metadata: metadata key-value pairs
//   - flag: metadata operation flag (Overwrite or Merge)
func (c *Client) SetMetadata(ctx context.Context, fileID string, metadata map[string]string, flag MetadataFlag) error {
	if err := c.checkClosed(); err != nil {
		return err
	}

	return c.setMetadataWithRetry(ctx, fileID, metadata, flag)
}

// GetMetadata retrieves metadata for a file.
func (c *Client) GetMetadata(ctx context.Context, fileID string) (map[string]string, error) {
	if err := c.checkClosed(); err != nil {
		return nil, err
	}

	return c.getMetadataWithRetry(ctx, fileID)
}

// GetFileInfo retrieves file information including size, create time, and CRC32.
func (c *Client) GetFileInfo(ctx context.Context, fileID string) (*FileInfo, error) {
	if err := c.checkClosed(); err != nil {
		return nil, err
	}

	return c.getFileInfoWithRetry(ctx, fileID)
}

// FileExists checks if a file exists on the storage server.
func (c *Client) FileExists(ctx context.Context, fileID string) (bool, error) {
	if err := c.checkClosed(); err != nil {
		return false, err
	}

	_, err := c.GetFileInfo(ctx, fileID)
	if err != nil {
		if errors.Is(err, ErrFileNotFound) {
			return false, nil
		}
		return false, err
	}
	return true, nil
}

// Close closes the client and releases all resources.
func (c *Client) Close() error {
	c.mu.Lock()
	defer c.mu.Unlock()

	if c.closed {
		return nil
	}

	c.closed = true

	var errs []error
	if c.trackerPool != nil {
		if err := c.trackerPool.Close(); err != nil {
			errs = append(errs, fmt.Errorf("tracker pool: %w", err))
		}
	}
	if c.storagePool != nil {
		if err := c.storagePool.Close(); err != nil {
			errs = append(errs, fmt.Errorf("storage pool: %w", err))
		}
	}

	if len(errs) > 0 {
		return fmt.Errorf("close errors: %v", errs)
	}
	return nil
}

// checkClosed returns an error if the client is closed.
func (c *Client) checkClosed() error {
	c.mu.RLock()
	defer c.mu.RUnlock()

	if c.closed {
		return ErrClientClosed
	}
	return nil
}

// validateConfig validates the client configuration.
func validateConfig(config *ClientConfig) error {
	if config == nil {
		return errors.New("config is nil")
	}
	if len(config.TrackerAddrs) == 0 {
		return errors.New("tracker addresses are required")
	}
	for _, addr := range config.TrackerAddrs {
		if addr == "" {
			return errors.New("tracker address cannot be empty")
		}
	}
	return nil
}
