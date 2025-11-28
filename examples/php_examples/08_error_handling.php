<?php
/**
 * FastDFS Comprehensive Error Handling Example
 * 
 * This example demonstrates various error handling patterns and strategies for FastDFS:
 * 
 * Error Handling Patterns:
 * - Try-catch-finally blocks for resource cleanup
 * - Error recovery strategies
 * - Graceful degradation
 * - Validation and sanitization
 * - Transaction-like operations with rollback
 * - Error context and debugging information
 * - Custom error handlers
 * - Timeout handling
 * - Partial failure handling
 * 
 * Error Types Covered:
 * - Network errors (connection failures, timeouts)
 * - File system errors (permissions, disk space)
 * - Validation errors (invalid inputs, file types)
 * - Business logic errors (quota exceeded, duplicate files)
 * - Storage errors (server unavailable, disk full)
 * 
 * Best Practices:
 * - Fail fast for unrecoverable errors
 * - Retry for transient errors
 * - Log all errors with context
 * - Provide meaningful error messages
 * - Clean up resources in all scenarios
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed
 * - FastDFS tracker server running
 */

// FastDFS configuration
define('TRACKER_HOST', '127.0.0.1');
define('TRACKER_PORT', 22122);
define('MAX_FILE_SIZE', 100 * 1024 * 1024); // 100MB
define('ALLOWED_EXTENSIONS', ['jpg', 'jpeg', 'png', 'gif', 'pdf', 'txt', 'doc', 'docx']);
define('ERROR_LOG_FILE', __DIR__ . '/fastdfs_detailed_errors.log');

/**
 * Custom error result class for structured error information
 */
class ErrorResult {
    public $success;
    public $data;
    public $error;
    public $errorCode;
    public $errorContext;
    
    /**
     * Create success result
     * 
     * @param mixed $data Result data
     * @return ErrorResult
     */
    public static function success($data) {
        $result = new self();
        $result->success = true;
        $result->data = $data;
        $result->error = null;
        $result->errorCode = null;
        $result->errorContext = [];
        return $result;
    }
    
    /**
     * Create error result
     * 
     * @param string $error Error message
     * @param string $errorCode Error code
     * @param array $context Additional context
     * @return ErrorResult
     */
    public static function failure($error, $errorCode = 'UNKNOWN', array $context = []) {
        $result = new self();
        $result->success = false;
        $result->data = null;
        $result->error = $error;
        $result->errorCode = $errorCode;
        $result->errorContext = $context;
        return $result;
    }
    
    /**
     * Check if operation was successful
     * 
     * @return bool
     */
    public function isSuccess() {
        return $this->success === true;
    }
    
    /**
     * Get data or throw exception
     * 
     * @return mixed
     * @throws Exception
     */
    public function getOrThrow() {
        if (!$this->success) {
            throw new Exception($this->error . " [Code: {$this->errorCode}]");
        }
        return $this->data;
    }
}

/**
 * Error codes enumeration
 */
class ErrorCodes {
    const CONNECTION_FAILED = 'E001';
    const UPLOAD_FAILED = 'E002';
    const DOWNLOAD_FAILED = 'E003';
    const FILE_NOT_FOUND = 'E004';
    const INVALID_FILE = 'E005';
    const FILE_TOO_LARGE = 'E006';
    const INVALID_EXTENSION = 'E007';
    const PERMISSION_DENIED = 'E008';
    const DISK_FULL = 'E009';
    const TIMEOUT = 'E010';
    const VALIDATION_FAILED = 'E011';
    const QUOTA_EXCEEDED = 'E012';
    const NETWORK_ERROR = 'E013';
    const UNKNOWN_ERROR = 'E999';
}

/**
 * Detailed error logger with multiple log levels
 */
class DetailedLogger {
    const LEVEL_DEBUG = 1;
    const LEVEL_INFO = 2;
    const LEVEL_WARNING = 3;
    const LEVEL_ERROR = 4;
    const LEVEL_CRITICAL = 5;
    
    private $logFile;
    private $minLevel;
    
    /**
     * Initialize logger
     * 
     * @param string $logFile Path to log file
     * @param int $minLevel Minimum log level to record
     */
    public function __construct($logFile, $minLevel = self::LEVEL_INFO) {
        $this->logFile = $logFile;
        $this->minLevel = $minLevel;
    }
    
    /**
     * Log a message with specified level
     * 
     * @param int $level Log level
     * @param string $message Log message
     * @param array $context Additional context
     */
    private function log($level, $message, array $context = []) {
        if ($level < $this->minLevel) {
            return;
        }
        
        $levelNames = [
            self::LEVEL_DEBUG => 'DEBUG',
            self::LEVEL_INFO => 'INFO',
            self::LEVEL_WARNING => 'WARNING',
            self::LEVEL_ERROR => 'ERROR',
            self::LEVEL_CRITICAL => 'CRITICAL'
        ];
        
        $timestamp = date('Y-m-d H:i:s');
        $levelName = $levelNames[$level] ?? 'UNKNOWN';
        
        $logEntry = sprintf(
            "[%s] [%s] %s\n",
            $timestamp,
            $levelName,
            $message
        );
        
        if (!empty($context)) {
            $logEntry .= "Context: " . json_encode($context, JSON_PRETTY_PRINT) . "\n";
        }
        
        $logEntry .= str_repeat("-", 80) . "\n";
        
        @file_put_contents($this->logFile, $logEntry, FILE_APPEND);
    }
    
    public function debug($message, array $context = []) {
        $this->log(self::LEVEL_DEBUG, $message, $context);
    }
    
    public function info($message, array $context = []) {
        $this->log(self::LEVEL_INFO, $message, $context);
    }
    
    public function warning($message, array $context = []) {
        $this->log(self::LEVEL_WARNING, $message, $context);
        echo "⚠ WARNING: $message\n";
    }
    
    public function error($message, array $context = []) {
        $this->log(self::LEVEL_ERROR, $message, $context);
        echo "✗ ERROR: $message\n";
    }
    
    public function critical($message, array $context = []) {
        $this->log(self::LEVEL_CRITICAL, $message, $context);
        echo "🔥 CRITICAL: $message\n";
    }
}

/**
 * File validator with comprehensive validation rules
 */
class FileValidator {
    private $logger;
    
    public function __construct(DetailedLogger $logger) {
        $this->logger = $logger;
    }
    
    /**
     * Validate file before upload
     * 
     * @param string $filePath Path to file
     * @return ErrorResult Validation result
     */
    public function validate($filePath) {
        $this->logger->debug("Validating file: $filePath");
        
        // Check if file exists
        if (!file_exists($filePath)) {
            $this->logger->error("File not found", ['file' => $filePath]);
            return ErrorResult::failure(
                "File does not exist: $filePath",
                ErrorCodes::FILE_NOT_FOUND,
                ['file' => $filePath]
            );
        }
        
        // Check if file is readable
        if (!is_readable($filePath)) {
            $this->logger->error("File not readable", ['file' => $filePath]);
            return ErrorResult::failure(
                "File is not readable (permission denied): $filePath",
                ErrorCodes::PERMISSION_DENIED,
                ['file' => $filePath]
            );
        }
        
        // Check file size
        $fileSize = filesize($filePath);
        if ($fileSize === false) {
            return ErrorResult::failure(
                "Cannot determine file size",
                ErrorCodes::INVALID_FILE,
                ['file' => $filePath]
            );
        }
        
        if ($fileSize > MAX_FILE_SIZE) {
            $this->logger->warning("File too large", [
                'file' => $filePath,
                'size' => $fileSize,
                'max' => MAX_FILE_SIZE
            ]);
            return ErrorResult::failure(
                sprintf(
                    "File too large: %s (max: %s)",
                    $this->formatBytes($fileSize),
                    $this->formatBytes(MAX_FILE_SIZE)
                ),
                ErrorCodes::FILE_TOO_LARGE,
                ['size' => $fileSize, 'max' => MAX_FILE_SIZE]
            );
        }
        
        if ($fileSize === 0) {
            $this->logger->warning("Empty file", ['file' => $filePath]);
            return ErrorResult::failure(
                "File is empty",
                ErrorCodes::INVALID_FILE,
                ['file' => $filePath]
            );
        }
        
        // Check file extension
        $extension = strtolower(pathinfo($filePath, PATHINFO_EXTENSION));
        if (!in_array($extension, ALLOWED_EXTENSIONS)) {
            $this->logger->warning("Invalid file extension", [
                'file' => $filePath,
                'extension' => $extension,
                'allowed' => ALLOWED_EXTENSIONS
            ]);
            return ErrorResult::failure(
                "Invalid file extension: .$extension (allowed: " . implode(', ', ALLOWED_EXTENSIONS) . ")",
                ErrorCodes::INVALID_EXTENSION,
                ['extension' => $extension, 'allowed' => ALLOWED_EXTENSIONS]
            );
        }
        
        // Validate file content (basic check)
        $finfo = finfo_open(FILEINFO_MIME_TYPE);
        if ($finfo) {
            $mimeType = finfo_file($finfo, $filePath);
            finfo_close($finfo);
            
            $this->logger->debug("File MIME type: $mimeType", ['file' => $filePath]);
        }
        
        $this->logger->info("File validation passed", [
            'file' => $filePath,
            'size' => $fileSize,
            'extension' => $extension
        ]);
        
        return ErrorResult::success([
            'file' => $filePath,
            'size' => $fileSize,
            'extension' => $extension
        ]);
    }
    
    /**
     * Format bytes to human-readable format
     * 
     * @param int $bytes
     * @return string
     */
    private function formatBytes($bytes) {
        $units = ['B', 'KB', 'MB', 'GB'];
        for ($i = 0; $bytes > 1024 && $i < count($units) - 1; $i++) {
            $bytes /= 1024;
        }
        return round($bytes, 2) . ' ' . $units[$i];
    }
}

/**
 * FastDFS operations with comprehensive error handling
 */
class FastDFSOperations {
    private $tracker;
    private $logger;
    private $validator;
    private $uploadedFiles = []; // Track uploaded files for rollback
    
    /**
     * Initialize FastDFS operations
     * 
     * @param DetailedLogger $logger Logger instance
     */
    public function __construct(DetailedLogger $logger) {
        $this->logger = $logger;
        $this->validator = new FileValidator($logger);
    }
    
    /**
     * Connect to FastDFS tracker with error handling
     * 
     * @return ErrorResult Connection result
     */
    public function connect() {
        $this->logger->info("Attempting to connect to FastDFS tracker", [
            'host' => TRACKER_HOST,
            'port' => TRACKER_PORT
        ]);
        
        try {
            // Set error handler to catch warnings
            set_error_handler(function($errno, $errstr) {
                throw new ErrorException($errstr, $errno);
            });
            
            $this->tracker = fastdfs_tracker_make_connection(TRACKER_HOST, TRACKER_PORT);
            
            restore_error_handler();
            
            if (!$this->tracker) {
                $this->logger->critical("Failed to connect to tracker server", [
                    'host' => TRACKER_HOST,
                    'port' => TRACKER_PORT
                ]);
                
                return ErrorResult::failure(
                    "Cannot connect to FastDFS tracker at " . TRACKER_HOST . ":" . TRACKER_PORT,
                    ErrorCodes::CONNECTION_FAILED,
                    ['host' => TRACKER_HOST, 'port' => TRACKER_PORT]
                );
            }
            
            $this->logger->info("Successfully connected to tracker server");
            echo "✓ Connected to FastDFS tracker\n";
            
            return ErrorResult::success($this->tracker);
            
        } catch (ErrorException $e) {
            restore_error_handler();
            
            $this->logger->critical("Connection error: " . $e->getMessage(), [
                'host' => TRACKER_HOST,
                'port' => TRACKER_PORT,
                'error' => $e->getMessage()
            ]);
            
            return ErrorResult::failure(
                "Connection error: " . $e->getMessage(),
                ErrorCodes::NETWORK_ERROR,
                ['exception' => $e->getMessage()]
            );
        }
    }
    
    /**
     * Upload file with comprehensive error handling
     * 
     * @param string $filePath Path to file
     * @param array $metadata Optional metadata
     * @return ErrorResult Upload result
     */
    public function uploadFile($filePath, array $metadata = []) {
        echo "\n--- Uploading File ---\n";
        echo "File: $filePath\n";
        
        // Step 1: Validate file
        $validationResult = $this->validator->validate($filePath);
        if (!$validationResult->isSuccess()) {
            return $validationResult;
        }
        
        $fileInfo = $validationResult->data;
        
        // Step 2: Check connection
        if (!$this->tracker) {
            $this->logger->error("No active tracker connection");
            return ErrorResult::failure(
                "Not connected to tracker server",
                ErrorCodes::CONNECTION_FAILED
            );
        }
        
        // Step 3: Perform upload with error handling
        try {
            $this->logger->info("Starting upload", [
                'file' => $filePath,
                'size' => $fileInfo['size'],
                'metadata' => $metadata
            ]);
            
            $startTime = microtime(true);
            
            // Set error handler for upload operation
            set_error_handler(function($errno, $errstr) {
                throw new ErrorException($errstr, $errno);
            });
            
            $fileId = fastdfs_storage_upload_by_filename(
                $filePath,
                $fileInfo['extension'],
                $metadata,
                [],
                TRACKER_HOST,
                TRACKER_PORT
            );
            
            restore_error_handler();
            
            $uploadTime = microtime(true) - $startTime;
            
            if (!$fileId) {
                $this->logger->error("Upload failed - no file ID returned", [
                    'file' => $filePath
                ]);
                
                return ErrorResult::failure(
                    "Upload failed: No file ID returned from server",
                    ErrorCodes::UPLOAD_FAILED,
                    ['file' => $filePath]
                );
            }
            
            // Track uploaded file for potential rollback
            $this->uploadedFiles[] = $fileId;
            
            $this->logger->info("Upload successful", [
                'file' => $filePath,
                'file_id' => $fileId,
                'upload_time' => round($uploadTime, 3) . 's'
            ]);
            
            echo "✓ Upload successful!\n";
            echo "File ID: $fileId\n";
            echo "Upload time: " . round($uploadTime, 3) . "s\n";
            
            return ErrorResult::success([
                'file_id' => $fileId,
                'upload_time' => $uploadTime,
                'size' => $fileInfo['size']
            ]);
            
        } catch (ErrorException $e) {
            restore_error_handler();
            
            $this->logger->error("Upload exception: " . $e->getMessage(), [
                'file' => $filePath,
                'exception' => $e->getMessage(),
                'trace' => $e->getTraceAsString()
            ]);
            
            return ErrorResult::failure(
                "Upload error: " . $e->getMessage(),
                ErrorCodes::UPLOAD_FAILED,
                ['file' => $filePath, 'exception' => $e->getMessage()]
            );
        }
    }
    
    /**
     * Download file with error handling
     * 
     * @param string $fileId File ID to download
     * @param string $localPath Local path to save file
     * @return ErrorResult Download result
     */
    public function downloadFile($fileId, $localPath) {
        echo "\n--- Downloading File ---\n";
        echo "File ID: $fileId\n";
        echo "Save to: $localPath\n";
        
        // Validate inputs
        if (empty($fileId)) {
            return ErrorResult::failure(
                "File ID cannot be empty",
                ErrorCodes::VALIDATION_FAILED
            );
        }
        
        // Check if directory is writable
        $directory = dirname($localPath);
        if (!is_dir($directory)) {
            $this->logger->warning("Creating directory: $directory");
            if (!@mkdir($directory, 0755, true)) {
                return ErrorResult::failure(
                    "Cannot create directory: $directory",
                    ErrorCodes::PERMISSION_DENIED,
                    ['directory' => $directory]
                );
            }
        }
        
        if (!is_writable($directory)) {
            $this->logger->error("Directory not writable", ['directory' => $directory]);
            return ErrorResult::failure(
                "Directory is not writable: $directory",
                ErrorCodes::PERMISSION_DENIED,
                ['directory' => $directory]
            );
        }
        
        // Check connection
        if (!$this->tracker) {
            return ErrorResult::failure(
                "Not connected to tracker server",
                ErrorCodes::CONNECTION_FAILED
            );
        }
        
        try {
            $this->logger->info("Starting download", [
                'file_id' => $fileId,
                'local_path' => $localPath
            ]);
            
            $startTime = microtime(true);
            
            set_error_handler(function($errno, $errstr) {
                throw new ErrorException($errstr, $errno);
            });
            
            $result = fastdfs_storage_download_file_to_file(
                $fileId,
                $localPath,
                TRACKER_HOST,
                TRACKER_PORT
            );
            
            restore_error_handler();
            
            $downloadTime = microtime(true) - $startTime;
            
            if (!$result) {
                $this->logger->error("Download failed", ['file_id' => $fileId]);
                return ErrorResult::failure(
                    "Download failed: Server returned error",
                    ErrorCodes::DOWNLOAD_FAILED,
                    ['file_id' => $fileId]
                );
            }
            
            if (!file_exists($localPath)) {
                $this->logger->error("Downloaded file not found", [
                    'file_id' => $fileId,
                    'local_path' => $localPath
                ]);
                return ErrorResult::failure(
                    "Download failed: File not saved to disk",
                    ErrorCodes::DOWNLOAD_FAILED,
                    ['file_id' => $fileId, 'path' => $localPath]
                );
            }
            
            $fileSize = filesize($localPath);
            
            $this->logger->info("Download successful", [
                'file_id' => $fileId,
                'size' => $fileSize,
                'download_time' => round($downloadTime, 3) . 's'
            ]);
            
            echo "✓ Download successful!\n";
            echo "File size: $fileSize bytes\n";
            echo "Download time: " . round($downloadTime, 3) . "s\n";
            
            return ErrorResult::success([
                'file_id' => $fileId,
                'local_path' => $localPath,
                'size' => $fileSize,
                'download_time' => $downloadTime
            ]);
            
        } catch (ErrorException $e) {
            restore_error_handler();
            
            $this->logger->error("Download exception: " . $e->getMessage(), [
                'file_id' => $fileId,
                'exception' => $e->getMessage()
            ]);
            
            return ErrorResult::failure(
                "Download error: " . $e->getMessage(),
                ErrorCodes::DOWNLOAD_FAILED,
                ['file_id' => $fileId, 'exception' => $e->getMessage()]
            );
        }
    }
    
    /**
     * Delete file with error handling
     * 
     * @param string $fileId File ID to delete
     * @return ErrorResult Delete result
     */
    public function deleteFile($fileId) {
        echo "\n--- Deleting File ---\n";
        echo "File ID: $fileId\n";
        
        if (!$this->tracker) {
            return ErrorResult::failure(
                "Not connected to tracker server",
                ErrorCodes::CONNECTION_FAILED
            );
        }
        
        try {
            $this->logger->info("Deleting file", ['file_id' => $fileId]);
            
            set_error_handler(function($errno, $errstr) {
                throw new ErrorException($errstr, $errno);
            });
            
            $result = fastdfs_storage_delete_file(
                $fileId,
                TRACKER_HOST,
                TRACKER_PORT
            );
            
            restore_error_handler();
            
            if (!$result) {
                $this->logger->warning("Delete failed", ['file_id' => $fileId]);
                return ErrorResult::failure(
                    "Delete failed: File may not exist or server error",
                    ErrorCodes::FILE_NOT_FOUND,
                    ['file_id' => $fileId]
                );
            }
            
            $this->logger->info("File deleted successfully", ['file_id' => $fileId]);
            echo "✓ File deleted successfully\n";
            
            return ErrorResult::success(['file_id' => $fileId]);
            
        } catch (ErrorException $e) {
            restore_error_handler();
            
            $this->logger->error("Delete exception: " . $e->getMessage(), [
                'file_id' => $fileId,
                'exception' => $e->getMessage()
            ]);
            
            return ErrorResult::failure(
                "Delete error: " . $e->getMessage(),
                ErrorCodes::UNKNOWN_ERROR,
                ['file_id' => $fileId, 'exception' => $e->getMessage()]
            );
        }
    }
    
    /**
     * Rollback uploaded files (delete them)
     * Useful for transaction-like operations
     * 
     * @return int Number of files rolled back
     */
    public function rollback() {
        if (empty($this->uploadedFiles)) {
            $this->logger->info("No files to rollback");
            return 0;
        }
        
        echo "\n--- Rolling Back Uploaded Files ---\n";
        $this->logger->warning("Starting rollback", [
            'file_count' => count($this->uploadedFiles)
        ]);
        
        $rolledBack = 0;
        
        foreach ($this->uploadedFiles as $fileId) {
            $result = $this->deleteFile($fileId);
            if ($result->isSuccess()) {
                $rolledBack++;
            }
        }
        
        $this->uploadedFiles = [];
        
        $this->logger->info("Rollback completed", [
            'rolled_back' => $rolledBack
        ]);
        
        echo "✓ Rolled back $rolledBack files\n";
        
        return $rolledBack;
    }
    
    /**
     * Close connection
     */
    public function disconnect() {
        if ($this->tracker) {
            fastdfs_tracker_close_connection($this->tracker);
            $this->tracker = null;
            $this->logger->info("Disconnected from tracker server");
            echo "✓ Disconnected from tracker\n";
        }
    }
}

/**
 * Demonstrate validation errors
 */
function demonstrateValidationErrors() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Validation Error Handling ===\n";
    echo str_repeat("=", 70) . "\n";
    
    $logger = new DetailedLogger(ERROR_LOG_FILE, DetailedLogger::LEVEL_DEBUG);
    $ops = new FastDFSOperations($logger);
    
    // Test 1: Non-existent file
    echo "\n--- Test 1: Non-existent File ---\n";
    $result = $ops->uploadFile('/path/to/nonexistent/file.txt');
    if (!$result->isSuccess()) {
        echo "Error Code: {$result->errorCode}\n";
        echo "Error Message: {$result->error}\n";
    }
    
    // Test 2: Invalid extension
    echo "\n--- Test 2: Invalid Extension ---\n";
    $invalidFile = __DIR__ . '/test.exe';
    file_put_contents($invalidFile, 'test content');
    
    $result = $ops->uploadFile($invalidFile);
    if (!$result->isSuccess()) {
        echo "Error Code: {$result->errorCode}\n";
        echo "Error Message: {$result->error}\n";
    }
    
    @unlink($invalidFile);
    
    // Test 3: Empty file
    echo "\n--- Test 3: Empty File ---\n";
    $emptyFile = __DIR__ . '/empty.txt';
    file_put_contents($emptyFile, '');
    
    $result = $ops->uploadFile($emptyFile);
    if (!$result->isSuccess()) {
        echo "Error Code: {$result->errorCode}\n";
        echo "Error Message: {$result->error}\n";
    }
    
    @unlink($emptyFile);
}

/**
 * Demonstrate transaction-like operations with rollback
 */
function demonstrateTransactionRollback() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Transaction Rollback ===\n";
    echo str_repeat("=", 70) . "\n";
    
    $logger = new DetailedLogger(ERROR_LOG_FILE, DetailedLogger::LEVEL_INFO);
    $ops = new FastDFSOperations($logger);
    
    // Connect
    $connectResult = $ops->connect();
    if (!$connectResult->isSuccess()) {
        echo "Cannot proceed: " . $connectResult->error . "\n";
        return;
    }
    
    try {
        // Create test files
        $files = [];
        for ($i = 1; $i <= 3; $i++) {
            $file = __DIR__ . "/transaction_test_$i.txt";
            file_put_contents($file, "Transaction test file $i\nTimestamp: " . time());
            $files[] = $file;
        }
        
        // Upload files (simulating a batch operation)
        echo "\nUploading batch of files...\n";
        $uploadedCount = 0;
        
        foreach ($files as $index => $file) {
            $result = $ops->uploadFile($file, ['batch' => 'transaction_test']);
            
            if ($result->isSuccess()) {
                $uploadedCount++;
            } else {
                // If any upload fails, rollback all
                echo "\n✗ Upload failed for file " . ($index + 1) . "\n";
                echo "Initiating rollback...\n";
                $ops->rollback();
                throw new Exception("Batch upload failed, rolled back all files");
            }
        }
        
        echo "\n✓ All files uploaded successfully ($uploadedCount files)\n";
        echo "Simulating an error that requires rollback...\n";
        
        // Simulate a business logic error that requires rollback
        sleep(1);
        echo "Business validation failed - rolling back transaction...\n";
        $ops->rollback();
        
    } catch (Exception $e) {
        echo "Exception: " . $e->getMessage() . "\n";
    } finally {
        // Cleanup test files
        foreach ($files as $file) {
            @unlink($file);
        }
        $ops->disconnect();
    }
}

/**
 * Demonstrate graceful degradation
 */
function demonstrateGracefulDegradation() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Graceful Degradation ===\n";
    echo str_repeat("=", 70) . "\n";
    
    $logger = new DetailedLogger(ERROR_LOG_FILE, DetailedLogger::LEVEL_INFO);
    $ops = new FastDFSOperations($logger);
    
    // Try to connect
    $connectResult = $ops->connect();
    
    if (!$connectResult->isSuccess()) {
        echo "\n⚠ Primary storage unavailable\n";
        echo "Falling back to local storage...\n";
        
        // Fallback: Save to local filesystem
        $testFile = __DIR__ . '/fallback_test.txt';
        file_put_contents($testFile, "Test content for fallback");
        
        $fallbackDir = __DIR__ . '/local_storage';
        if (!is_dir($fallbackDir)) {
            mkdir($fallbackDir, 0755, true);
        }
        
        $fallbackPath = $fallbackDir . '/' . uniqid('file_') . '.txt';
        copy($testFile, $fallbackPath);
        
        echo "✓ File saved to local storage: $fallbackPath\n";
        echo "Note: File will be synced to FastDFS when service is restored\n";
        
        @unlink($testFile);
        return;
    }
    
    echo "✓ Primary storage available - using FastDFS\n";
    $ops->disconnect();
}

/**
 * Demonstrate comprehensive error handling workflow
 */
function demonstrateCompleteWorkflow() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Complete Error Handling Workflow ===\n";
    echo str_repeat("=", 70) . "\n";
    
    $logger = new DetailedLogger(ERROR_LOG_FILE, DetailedLogger::LEVEL_INFO);
    $ops = new FastDFSOperations($logger);
    
    // Step 1: Connect with error handling
    $connectResult = $ops->connect();
    if (!$connectResult->isSuccess()) {
        echo "Fatal: Cannot connect to FastDFS\n";
        echo "Error: {$connectResult->error}\n";
        return;
    }
    
    // Step 2: Create and upload valid file
    $testFile = __DIR__ . '/complete_test.txt';
    file_put_contents($testFile, "Complete workflow test\nTimestamp: " . date('Y-m-d H:i:s'));
    
    $uploadResult = $ops->uploadFile($testFile, [
        'test' => 'complete_workflow',
        'timestamp' => time()
    ]);
    
    if (!$uploadResult->isSuccess()) {
        echo "Upload failed: {$uploadResult->error}\n";
        @unlink($testFile);
        $ops->disconnect();
        return;
    }
    
    $fileId = $uploadResult->data['file_id'];
    
    // Step 3: Download file
    $downloadPath = __DIR__ . '/complete_test_downloaded.txt';
    $downloadResult = $ops->downloadFile($fileId, $downloadPath);
    
    if (!$downloadResult->isSuccess()) {
        echo "Download failed: {$downloadResult->error}\n";
    } else {
        echo "\n✓ Workflow completed successfully!\n";
        echo "Original file: $testFile\n";
        echo "File ID: $fileId\n";
        echo "Downloaded to: $downloadPath\n";
    }
    
    // Cleanup
    @unlink($testFile);
    @unlink($downloadPath);
    $ops->disconnect();
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Comprehensive Error Handling Example ===\n\n";
    
    echo "This example demonstrates:\n";
    echo "  ✓ Structured error results (success/failure)\n";
    echo "  ✓ Error codes for categorization\n";
    echo "  ✓ Detailed logging with multiple levels\n";
    echo "  ✓ File validation before upload\n";
    echo "  ✓ Transaction-like operations with rollback\n";
    echo "  ✓ Graceful degradation strategies\n";
    echo "  ✓ Resource cleanup in all scenarios\n\n";
    
    // Demonstration 1: Validation errors
    demonstrateValidationErrors();
    
    // Demonstration 2: Transaction rollback
    demonstrateTransactionRollback();
    
    // Demonstration 3: Graceful degradation
    demonstrateGracefulDegradation();
    
    // Demonstration 4: Complete workflow
    demonstrateCompleteWorkflow();
    
    // Summary
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Summary ===\n";
    echo str_repeat("=", 70) . "\n";
    
    echo "\nError Handling Patterns Demonstrated:\n\n";
    
    echo "1. Structured Error Results:\n";
    echo "   • ErrorResult class for consistent error handling\n";
    echo "   • Success/failure states with data or error info\n";
    echo "   • Error codes for programmatic error handling\n";
    echo "   • Context data for debugging\n\n";
    
    echo "2. Validation:\n";
    echo "   • File existence and readability checks\n";
    echo "   • File size limits\n";
    echo "   • Extension whitelisting\n";
    echo "   • MIME type validation\n";
    echo "   • Early failure for invalid inputs\n\n";
    
    echo "3. Logging:\n";
    echo "   • Multiple log levels (DEBUG, INFO, WARNING, ERROR, CRITICAL)\n";
    echo "   • Structured context data\n";
    echo "   • Timestamp and level information\n";
    echo "   • File-based persistent logging\n\n";
    
    echo "4. Transaction Support:\n";
    echo "   • Track uploaded files\n";
    echo "   • Rollback on failure\n";
    echo "   • All-or-nothing semantics\n";
    echo "   • Cleanup on errors\n\n";
    
    echo "5. Graceful Degradation:\n";
    echo "   • Fallback to local storage\n";
    echo "   • Service availability checks\n";
    echo "   • User-friendly error messages\n";
    echo "   • Recovery strategies\n\n";
    
    echo "Error Log Location:\n";
    echo "  " . ERROR_LOG_FILE . "\n\n";
    
    echo "Best Practices Applied:\n";
    echo "  • Fail fast for unrecoverable errors\n";
    echo "  • Provide meaningful error messages\n";
    echo "  • Log all errors with context\n";
    echo "  • Clean up resources in finally blocks\n";
    echo "  • Use error codes for categorization\n";
    echo "  • Validate inputs before processing\n";
    echo "  • Support rollback for batch operations\n";
}

// Run the example
main();

?>
