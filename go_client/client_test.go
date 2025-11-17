package fdfs

import (
	"context"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestNewClient(t *testing.T) {
	tests := []struct {
		name    string
		config  *ClientConfig
		wantErr bool
	}{
		{
			name: "valid config",
			config: &ClientConfig{
				TrackerAddrs: []string{"192.168.1.100:22122"},
			},
			wantErr: false,
		},
		{
			name:    "nil config",
			config:  nil,
			wantErr: true,
		},
		{
			name: "empty tracker addrs",
			config: &ClientConfig{
				TrackerAddrs: []string{},
			},
			wantErr: true,
		},
		{
			name: "empty tracker addr string",
			config: &ClientConfig{
				TrackerAddrs: []string{""},
			},
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			client, err := NewClient(tt.config)
			if tt.wantErr {
				assert.Error(t, err)
				assert.Nil(t, client)
			} else {
				assert.NoError(t, err)
				assert.NotNil(t, client)
				if client != nil {
					client.Close()
				}
			}
		})
	}
}

func TestClientDefaults(t *testing.T) {
	config := &ClientConfig{
		TrackerAddrs: []string{"192.168.1.100:22122"},
	}

	client, err := NewClient(config)
	require.NoError(t, err)
	defer client.Close()

	assert.Equal(t, 10, client.config.MaxConns)
	assert.Equal(t, 5*time.Second, client.config.ConnectTimeout)
	assert.Equal(t, 30*time.Second, client.config.NetworkTimeout)
	assert.Equal(t, 60*time.Second, client.config.IdleTimeout)
	assert.Equal(t, 3, client.config.RetryCount)
}

func TestSplitFileID(t *testing.T) {
	tests := []struct {
		name         string
		fileID       string
		wantGroup    string
		wantFilename string
		wantErr      bool
	}{
		{
			name:         "valid file ID",
			fileID:       "group1/M00/00/00/test.jpg",
			wantGroup:    "group1",
			wantFilename: "M00/00/00/test.jpg",
			wantErr:      false,
		},
		{
			name:    "empty file ID",
			fileID:  "",
			wantErr: true,
		},
		{
			name:    "no separator",
			fileID:  "group1",
			wantErr: true,
		},
		{
			name:    "empty group",
			fileID:  "/M00/00/00/test.jpg",
			wantErr: true,
		},
		{
			name:    "empty filename",
			fileID:  "group1/",
			wantErr: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			group, filename, err := splitFileID(tt.fileID)
			if tt.wantErr {
				assert.Error(t, err)
			} else {
				assert.NoError(t, err)
				assert.Equal(t, tt.wantGroup, group)
				assert.Equal(t, tt.wantFilename, filename)
			}
		})
	}
}

func TestJoinFileID(t *testing.T) {
	tests := []struct {
		name       string
		groupName  string
		filename   string
		wantFileID string
	}{
		{
			name:       "normal case",
			groupName:  "group1",
			filename:   "M00/00/00/test.jpg",
			wantFileID: "group1/M00/00/00/test.jpg",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			fileID := joinFileID(tt.groupName, tt.filename)
			assert.Equal(t, tt.wantFileID, fileID)
		})
	}
}

func TestGetFileExtName(t *testing.T) {
	tests := []struct {
		name     string
		filename string
		want     string
	}{
		{
			name:     "jpg extension",
			filename: "test.jpg",
			want:     "jpg",
		},
		{
			name:     "multiple dots",
			filename: "test.file.txt",
			want:     "txt",
		},
		{
			name:     "no extension",
			filename: "testfile",
			want:     "",
		},
		{
			name:     "long extension truncated",
			filename: "test.verylongext",
			want:     "verylo", // 6 characters max
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			ext := getFileExtName(tt.filename)
			assert.Equal(t, tt.want, ext)
		})
	}
}

func TestEncodeDecodeMetadata(t *testing.T) {
	tests := []struct {
		name     string
		metadata map[string]string
	}{
		{
			name: "normal metadata",
			metadata: map[string]string{
				"author":  "John Doe",
				"date":    "2025-01-15",
				"version": "1.0",
			},
		},
		{
			name:     "empty metadata",
			metadata: map[string]string{},
		},
		{
			name:     "nil metadata",
			metadata: nil,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			encoded := encodeMetadata(tt.metadata)
			decoded, err := decodeMetadata(encoded)
			assert.NoError(t, err)

			if tt.metadata == nil || len(tt.metadata) == 0 {
				assert.Empty(t, decoded)
			} else {
				assert.Equal(t, len(tt.metadata), len(decoded))
				for key, value := range tt.metadata {
					assert.Equal(t, value, decoded[key])
				}
			}
		})
	}
}

func TestEncodeDecodeHeader(t *testing.T) {
	tests := []struct {
		name   string
		length int64
		cmd    byte
		status byte
	}{
		{
			name:   "normal header",
			length: 1024,
			cmd:    11,
			status: 0,
		},
		{
			name:   "zero length",
			length: 0,
			cmd:    12,
			status: 0,
		},
		{
			name:   "error status",
			length: 100,
			cmd:    13,
			status: 2,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			encoded := encodeHeader(tt.length, tt.cmd, tt.status)
			assert.Equal(t, FdfsProtoHeaderLen, len(encoded))

			decoded, err := decodeHeader(encoded)
			assert.NoError(t, err)
			assert.Equal(t, tt.length, decoded.Length)
			assert.Equal(t, tt.cmd, decoded.Cmd)
			assert.Equal(t, tt.status, decoded.Status)
		})
	}
}

func TestMapStatusToError(t *testing.T) {
	tests := []struct {
		name   string
		status byte
		want   error
	}{
		{
			name:   "success",
			status: 0,
			want:   nil,
		},
		{
			name:   "file not found",
			status: 2,
			want:   ErrFileNotFound,
		},
		{
			name:   "file already exists",
			status: 6,
			want:   ErrFileAlreadyExists,
		},
		{
			name:   "invalid argument",
			status: 22,
			want:   ErrInvalidArgument,
		},
		{
			name:   "insufficient space",
			status: 28,
			want:   ErrInsufficientSpace,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := mapStatusToError(tt.status)
			if tt.want == nil {
				assert.NoError(t, err)
			} else {
				assert.ErrorIs(t, err, tt.want)
			}
		})
	}
}

func TestClientClose(t *testing.T) {
	config := &ClientConfig{
		TrackerAddrs: []string{"192.168.1.100:22122"},
	}

	client, err := NewClient(config)
	require.NoError(t, err)

	// Close once
	err = client.Close()
	assert.NoError(t, err)

	// Close again should not error
	err = client.Close()
	assert.NoError(t, err)

	// Operations after close should fail
	ctx := context.Background()
	_, err = client.UploadBuffer(ctx, []byte("test"), "txt", nil)
	assert.ErrorIs(t, err, ErrClientClosed)
}
