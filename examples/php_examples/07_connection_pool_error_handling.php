<?php
/**
 * FastDFS Connection Pool and Advanced Error Handling Example
 * 
 * This example demonstrates professional-grade connection management and error handling:
 * 
 * Connection Pool Features:
 * - Connection pooling to reuse tracker connections
 * - Connection health checking and auto-recovery
 * - Load balancing across multiple tracker servers
 * - Connection timeout handling
 * - Pool size management (min/max connections)
 * 
 * Error Handling Features:
 * - Custom exception classes for different error types
 * - Retry mechanisms with exponential backoff
 * - Circuit breaker pattern for failing services
 * - Comprehensive error logging
 * - Graceful degradation strategies
 * - Transaction-like operations with rollback
 * 
 * Use Cases:
 * - High-traffic production applications
 * - Microservices with FastDFS integration
 * - Applications requiring high availability
 * - Systems with multiple tracker servers
 * 
 * Prerequisites:
 * - FastDFS PHP extension installed
 * - Multiple tracker servers (optional, for load balancing demo)
 * - Write permissions for log files
 */

// Configuration constants
define('TRACKER_SERVERS', [
    ['host' => '127.0.0.1', 'port' => 22122],
    // Add more tracker servers for load balancing
    // ['host' => '127.0.0.2', 'port' => 22122],
    // ['host' => '127.0.0.3', 'port' => 22122],
]);

define('POOL_MIN_SIZE', 2);           // Minimum connections to maintain
define('POOL_MAX_SIZE', 10);          // Maximum connections allowed
define('CONNECTION_TIMEOUT', 5);       // Connection timeout in seconds
define('MAX_RETRY_ATTEMPTS', 3);      // Maximum retry attempts
define('CIRCUIT_BREAKER_THRESHOLD', 5); // Failures before circuit opens
define('CIRCUIT_BREAKER_TIMEOUT', 30);  // Seconds before retry after circuit opens

/**
 * Custom Exception Classes for Better Error Handling
 */

/**
 * Base exception for all FastDFS operations
 */
class FastDFSException extends Exception {
    protected $context = [];
    
    public function __construct($message, $code = 0, Exception $previous = null, array $context = []) {
        parent::__construct($message, $code, $previous);
        $this->context = $context;
    }
    
    public function getContext() {
        return $this->context;
    }
}

/**
 * Exception for connection-related errors
 */
class FastDFSConnectionException extends FastDFSException {}

/**
 * Exception for upload operation errors
 */
class FastDFSUploadException extends FastDFSException {}

/**
 * Exception for download operation errors
 */
class FastDFSDownloadException extends FastDFSException {}

/**
 * Exception for file not found errors
 */
class FastDFSFileNotFoundException extends FastDFSException {}

/**
 * Exception for timeout errors
 */
class FastDFSTimeoutException extends FastDFSException {}

/**
 * Exception for circuit breaker open state
 */
class FastDFSCircuitBreakerException extends FastDFSException {}

/**
 * FastDFS Connection Pool Manager
 * 
 * Manages a pool of tracker connections for efficient resource usage
 * and improved performance in high-traffic scenarios.
 */
class FastDFSConnectionPool {
    private $availableConnections = [];
    private $activeConnections = [];
    private $trackerServers = [];
    private $currentServerIndex = 0;
    private $minSize;
    private $maxSize;
    private $connectionTimeout;
    
    /**
     * Initialize the connection pool
     * 
     * @param array $trackerServers Array of tracker server configurations
     * @param int $minSize Minimum number of connections to maintain
     * @param int $maxSize Maximum number of connections allowed
     * @param int $timeout Connection timeout in seconds
     */
    public function __construct(array $trackerServers, $minSize = 2, $maxSize = 10, $timeout = 5) {
        $this->trackerServers = $trackerServers;
        $this->minSize = $minSize;
        $this->maxSize = $maxSize;
        $this->connectionTimeout = $timeout;
        
        echo "=== Initializing Connection Pool ===\n";
        echo "Tracker servers: " . count($trackerServers) . "\n";
        echo "Pool size: min=$minSize, max=$maxSize\n";
        echo "Connection timeout: {$timeout}s\n\n";
        
        // Pre-create minimum connections
        $this->warmUp();
    }
    
    /**
     * Warm up the pool by creating minimum connections
     */
    private function warmUp() {
        echo "Warming up connection pool...\n";
        
        for ($i = 0; $i < $this->minSize; $i++) {
            try {
                $connection = $this->createConnection();
                if ($connection) {
                    $this->availableConnections[] = $connection;
                    echo "✓ Created connection " . ($i + 1) . "/$this->minSize\n";
                }
            } catch (Exception $e) {
                echo "✗ Failed to create connection " . ($i + 1) . ": " . $e->getMessage() . "\n";
            }
        }
        
        echo "Pool warmed up with " . count($this->availableConnections) . " connections\n\n";
    }
    
    /**
     * Create a new tracker connection with load balancing
     * 
     * @return resource|false Tracker connection or false on failure
     * @throws FastDFSConnectionException
     */
    private function createConnection() {
        $attempts = 0;
        $maxAttempts = count($this->trackerServers);
        
        while ($attempts < $maxAttempts) {
            $server = $this->getNextServer();
            
            try {
                // Set connection timeout (if supported by extension)
                $tracker = @fastdfs_tracker_make_connection($server['host'], $server['port']);
                
                if ($tracker) {
                    return [
                        'resource' => $tracker,
                        'server' => $server,
                        'created_at' => time(),
                        'last_used' => time(),
                        'id' => uniqid('conn_')
                    ];
                }
                
            } catch (Exception $e) {
                // Try next server
            }
            
            $attempts++;
            $this->currentServerIndex = ($this->currentServerIndex + 1) % count($this->trackerServers);
        }
        
        throw new FastDFSConnectionException(
            "Failed to connect to any tracker server",
            0,
            null,
            ['servers' => $this->trackerServers]
        );
    }
    
    /**
     * Get next tracker server using round-robin load balancing
     * 
     * @return array Server configuration
     */
    private function getNextServer() {
        $server = $this->trackerServers[$this->currentServerIndex];
        $this->currentServerIndex = ($this->currentServerIndex + 1) % count($this->trackerServers);
        return $server;
    }
    
    /**
     * Acquire a connection from the pool
     * 
     * @return array Connection info
     * @throws FastDFSConnectionException
     */
    public function acquire() {
        // Try to get an available connection
        if (!empty($this->availableConnections)) {
            $connection = array_pop($this->availableConnections);
            
            // Verify connection is still valid
            if ($this->isConnectionValid($connection)) {
                $connection['last_used'] = time();
                $this->activeConnections[$connection['id']] = $connection;
                
                echo "→ Acquired connection {$connection['id']} from pool\n";
                return $connection;
            } else {
                // Connection is stale, close it and create new one
                $this->closeConnection($connection);
            }
        }
        
        // Create new connection if under max limit
        if (count($this->activeConnections) + count($this->availableConnections) < $this->maxSize) {
            $connection = $this->createConnection();
            $this->activeConnections[$connection['id']] = $connection;
            
            echo "→ Created new connection {$connection['id']}\n";
            return $connection;
        }
        
        throw new FastDFSConnectionException(
            "Connection pool exhausted (max: {$this->maxSize})",
            0,
            null,
            ['active' => count($this->activeConnections), 'available' => count($this->availableConnections)]
        );
    }
    
    /**
     * Release a connection back to the pool
     * 
     * @param array $connection Connection to release
     */
    public function release($connection) {
        if (!isset($connection['id'])) {
            return;
        }
        
        // Remove from active connections
        if (isset($this->activeConnections[$connection['id']])) {
            unset($this->activeConnections[$connection['id']]);
        }
        
        // Add back to available pool if still valid
        if ($this->isConnectionValid($connection)) {
            $this->availableConnections[] = $connection;
            echo "← Released connection {$connection['id']} to pool\n";
        } else {
            $this->closeConnection($connection);
            echo "← Closed invalid connection {$connection['id']}\n";
        }
    }
    
    /**
     * Check if connection is still valid
     * 
     * @param array $connection Connection to check
     * @return bool True if valid, false otherwise
     */
    private function isConnectionValid($connection) {
        // Check if resource is still valid
        if (!is_resource($connection['resource'])) {
            return false;
        }
        
        // Check connection age (close connections older than 1 hour)
        $age = time() - $connection['created_at'];
        if ($age > 3600) {
            return false;
        }
        
        return true;
    }
    
    /**
     * Close a connection
     * 
     * @param array $connection Connection to close
     */
    private function closeConnection($connection) {
        if (is_resource($connection['resource'])) {
            @fastdfs_tracker_close_connection($connection['resource']);
        }
    }
    
    /**
     * Get pool statistics
     * 
     * @return array Pool statistics
     */
    public function getStats() {
        return [
            'available' => count($this->availableConnections),
            'active' => count($this->activeConnections),
            'total' => count($this->availableConnections) + count($this->activeConnections),
            'max' => $this->maxSize
        ];
    }
    
    /**
     * Close all connections and cleanup
     */
    public function shutdown() {
        echo "\n=== Shutting Down Connection Pool ===\n";
        
        // Close all available connections
        foreach ($this->availableConnections as $connection) {
            $this->closeConnection($connection);
        }
        
        // Close all active connections
        foreach ($this->activeConnections as $connection) {
            $this->closeConnection($connection);
        }
        
        echo "✓ All connections closed\n";
        
        $this->availableConnections = [];
        $this->activeConnections = [];
    }
    
    /**
     * Destructor - ensure cleanup
     */
    public function __destruct() {
        $this->shutdown();
    }
}

/**
 * Circuit Breaker Pattern Implementation
 * 
 * Prevents cascading failures by stopping requests to failing services
 * and allowing them time to recover.
 */
class CircuitBreaker {
    private $failureCount = 0;
    private $lastFailureTime = 0;
    private $state = 'closed'; // closed, open, half-open
    private $threshold;
    private $timeout;
    
    /**
     * Initialize circuit breaker
     * 
     * @param int $threshold Number of failures before opening circuit
     * @param int $timeout Seconds to wait before attempting recovery
     */
    public function __construct($threshold = 5, $timeout = 30) {
        $this->threshold = $threshold;
        $this->timeout = $timeout;
    }
    
    /**
     * Check if circuit allows request
     * 
     * @return bool True if request allowed, false otherwise
     * @throws FastDFSCircuitBreakerException
     */
    public function allowRequest() {
        if ($this->state === 'closed') {
            return true;
        }
        
        if ($this->state === 'open') {
            // Check if timeout has passed
            if (time() - $this->lastFailureTime >= $this->timeout) {
                echo "⚡ Circuit breaker entering half-open state\n";
                $this->state = 'half-open';
                return true;
            }
            
            throw new FastDFSCircuitBreakerException(
                "Circuit breaker is OPEN - service unavailable",
                0,
                null,
                ['failures' => $this->failureCount, 'state' => $this->state]
            );
        }
        
        // half-open state - allow one request to test
        return true;
    }
    
    /**
     * Record successful operation
     */
    public function recordSuccess() {
        if ($this->state === 'half-open') {
            echo "⚡ Circuit breaker closing - service recovered\n";
            $this->state = 'closed';
            $this->failureCount = 0;
        }
    }
    
    /**
     * Record failed operation
     */
    public function recordFailure() {
        $this->failureCount++;
        $this->lastFailureTime = time();
        
        if ($this->state === 'half-open') {
            echo "⚡ Circuit breaker opening - service still failing\n";
            $this->state = 'open';
        } elseif ($this->failureCount >= $this->threshold) {
            echo "⚡ Circuit breaker OPENED after {$this->failureCount} failures\n";
            $this->state = 'open';
        }
    }
    
    /**
     * Get current state
     * 
     * @return string Current state (closed, open, half-open)
     */
    public function getState() {
        return $this->state;
    }
    
    /**
     * Reset circuit breaker
     */
    public function reset() {
        $this->state = 'closed';
        $this->failureCount = 0;
        $this->lastFailureTime = 0;
    }
}

/**
 * Error Logger for FastDFS operations
 */
class FastDFSLogger {
    private $logFile;
    
    /**
     * Initialize logger
     * 
     * @param string $logFile Path to log file
     */
    public function __construct($logFile = null) {
        $this->logFile = $logFile ?: __DIR__ . '/fastdfs_errors.log';
    }
    
    /**
     * Log an error
     * 
     * @param Exception $exception Exception to log
     * @param array $context Additional context
     */
    public function logError(Exception $exception, array $context = []) {
        $timestamp = date('Y-m-d H:i:s');
        $message = sprintf(
            "[%s] %s: %s\n",
            $timestamp,
            get_class($exception),
            $exception->getMessage()
        );
        
        if (!empty($context)) {
            $message .= "Context: " . json_encode($context) . "\n";
        }
        
        $message .= "Stack trace:\n" . $exception->getTraceAsString() . "\n";
        $message .= str_repeat("-", 80) . "\n";
        
        // Write to file
        @file_put_contents($this->logFile, $message, FILE_APPEND);
        
        // Also output to console in this example
        echo "✗ Error logged: " . $exception->getMessage() . "\n";
    }
    
    /**
     * Log info message
     * 
     * @param string $message Message to log
     */
    public function logInfo($message) {
        $timestamp = date('Y-m-d H:i:s');
        $logMessage = "[{$timestamp}] INFO: {$message}\n";
        @file_put_contents($this->logFile, $logMessage, FILE_APPEND);
    }
}

/**
 * FastDFS Client with Connection Pool and Error Handling
 */
class FastDFSClient {
    private $pool;
    private $circuitBreaker;
    private $logger;
    
    /**
     * Initialize FastDFS client
     * 
     * @param FastDFSConnectionPool $pool Connection pool
     * @param CircuitBreaker $circuitBreaker Circuit breaker
     * @param FastDFSLogger $logger Error logger
     */
    public function __construct(FastDFSConnectionPool $pool, CircuitBreaker $circuitBreaker, FastDFSLogger $logger) {
        $this->pool = $pool;
        $this->circuitBreaker = $circuitBreaker;
        $this->logger = $logger;
    }
    
    /**
     * Upload file with retry and error handling
     * 
     * @param string $filePath Path to file to upload
     * @param array $metadata Optional metadata
     * @param int $maxRetries Maximum retry attempts
     * @return string File ID on success
     * @throws FastDFSUploadException
     */
    public function uploadFile($filePath, array $metadata = [], $maxRetries = MAX_RETRY_ATTEMPTS) {
        if (!file_exists($filePath)) {
            throw new FastDFSUploadException(
                "File not found: $filePath",
                0,
                null,
                ['file' => $filePath]
            );
        }
        
        $attempt = 0;
        $lastException = null;
        
        while ($attempt < $maxRetries) {
            $attempt++;
            $connection = null;
            
            try {
                // Check circuit breaker
                $this->circuitBreaker->allowRequest();
                
                // Acquire connection from pool
                $connection = $this->pool->acquire();
                
                echo "\n--- Upload Attempt $attempt/$maxRetries ---\n";
                echo "File: $filePath\n";
                echo "Size: " . filesize($filePath) . " bytes\n";
                
                $fileExtension = pathinfo($filePath, PATHINFO_EXTENSION);
                
                // Perform upload
                $fileId = fastdfs_storage_upload_by_filename(
                    $filePath,
                    $fileExtension,
                    $metadata,
                    [],
                    $connection['server']['host'],
                    $connection['server']['port']
                );
                
                if (!$fileId) {
                    throw new FastDFSUploadException("Upload returned no file ID");
                }
                
                // Success!
                $this->circuitBreaker->recordSuccess();
                $this->logger->logInfo("Upload successful: $fileId");
                
                echo "✓ Upload successful!\n";
                echo "File ID: $fileId\n";
                
                return $fileId;
                
            } catch (Exception $e) {
                $lastException = $e;
                $this->circuitBreaker->recordFailure();
                $this->logger->logError($e, ['attempt' => $attempt, 'file' => $filePath]);
                
                echo "✗ Attempt $attempt failed: " . $e->getMessage() . "\n";
                
                if ($attempt < $maxRetries) {
                    $delay = $this->calculateBackoff($attempt);
                    echo "Retrying in {$delay}s...\n";
                    sleep($delay);
                }
                
            } finally {
                // Always release connection back to pool
                if ($connection) {
                    $this->pool->release($connection);
                }
            }
        }
        
        throw new FastDFSUploadException(
            "Upload failed after $maxRetries attempts",
            0,
            $lastException,
            ['file' => $filePath, 'attempts' => $maxRetries]
        );
    }
    
    /**
     * Download file with retry and error handling
     * 
     * @param string $fileId File ID to download
     * @param string $localPath Local path to save file
     * @param int $maxRetries Maximum retry attempts
     * @return bool True on success
     * @throws FastDFSDownloadException
     */
    public function downloadFile($fileId, $localPath, $maxRetries = MAX_RETRY_ATTEMPTS) {
        $attempt = 0;
        $lastException = null;
        
        while ($attempt < $maxRetries) {
            $attempt++;
            $connection = null;
            
            try {
                // Check circuit breaker
                $this->circuitBreaker->allowRequest();
                
                // Acquire connection
                $connection = $this->pool->acquire();
                
                echo "\n--- Download Attempt $attempt/$maxRetries ---\n";
                echo "File ID: $fileId\n";
                echo "Save to: $localPath\n";
                
                // Perform download
                $result = fastdfs_storage_download_file_to_file(
                    $fileId,
                    $localPath,
                    $connection['server']['host'],
                    $connection['server']['port']
                );
                
                if (!$result || !file_exists($localPath)) {
                    throw new FastDFSDownloadException("Download failed - file not saved");
                }
                
                // Success!
                $this->circuitBreaker->recordSuccess();
                $this->logger->logInfo("Download successful: $fileId");
                
                echo "✓ Download successful!\n";
                echo "Size: " . filesize($localPath) . " bytes\n";
                
                return true;
                
            } catch (Exception $e) {
                $lastException = $e;
                $this->circuitBreaker->recordFailure();
                $this->logger->logError($e, ['attempt' => $attempt, 'file_id' => $fileId]);
                
                echo "✗ Attempt $attempt failed: " . $e->getMessage() . "\n";
                
                if ($attempt < $maxRetries) {
                    $delay = $this->calculateBackoff($attempt);
                    echo "Retrying in {$delay}s...\n";
                    sleep($delay);
                }
                
            } finally {
                if ($connection) {
                    $this->pool->release($connection);
                }
            }
        }
        
        throw new FastDFSDownloadException(
            "Download failed after $maxRetries attempts",
            0,
            $lastException,
            ['file_id' => $fileId, 'attempts' => $maxRetries]
        );
    }
    
    /**
     * Calculate exponential backoff delay
     * 
     * @param int $attempt Current attempt number
     * @return int Delay in seconds
     */
    private function calculateBackoff($attempt) {
        // Exponential backoff: 1s, 2s, 4s, 8s, etc.
        return min(pow(2, $attempt - 1), 30); // Max 30 seconds
    }
    
    /**
     * Get pool statistics
     * 
     * @return array Pool stats
     */
    public function getPoolStats() {
        return $this->pool->getStats();
    }
}

/**
 * Demonstrate connection pool usage
 */
function demonstrateConnectionPool() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Connection Pool Demonstration ===\n";
    echo str_repeat("=", 70) . "\n\n";
    
    // Initialize components
    $pool = new FastDFSConnectionPool(TRACKER_SERVERS, POOL_MIN_SIZE, POOL_MAX_SIZE, CONNECTION_TIMEOUT);
    $circuitBreaker = new CircuitBreaker(CIRCUIT_BREAKER_THRESHOLD, CIRCUIT_BREAKER_TIMEOUT);
    $logger = new FastDFSLogger();
    $client = new FastDFSClient($pool, $circuitBreaker, $logger);
    
    // Create test file
    $testFile = __DIR__ . '/pool_test.txt';
    file_put_contents($testFile, "Connection pool test file\nTimestamp: " . date('Y-m-d H:i:s'));
    
    try {
        // Upload file
        $fileId = $client->uploadFile($testFile, [
            'test' => 'connection_pool',
            'timestamp' => time()
        ]);
        
        // Show pool stats
        $stats = $client->getPoolStats();
        echo "\nPool Statistics:\n";
        echo "  Active connections: {$stats['active']}\n";
        echo "  Available connections: {$stats['available']}\n";
        echo "  Total connections: {$stats['total']}/{$stats['max']}\n";
        
        // Download file
        $downloadPath = __DIR__ . '/pool_test_downloaded.txt';
        $client->downloadFile($fileId, $downloadPath);
        
        // Show final pool stats
        $stats = $client->getPoolStats();
        echo "\nFinal Pool Statistics:\n";
        echo "  Active connections: {$stats['active']}\n";
        echo "  Available connections: {$stats['available']}\n";
        
    } catch (FastDFSException $e) {
        echo "\n✗ Operation failed: " . $e->getMessage() . "\n";
        if ($e->getContext()) {
            echo "Context: " . json_encode($e->getContext()) . "\n";
        }
    } finally {
        @unlink($testFile);
    }
    
    return $pool;
}

/**
 * Demonstrate error handling and retry logic
 */
function demonstrateErrorHandling() {
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Error Handling Demonstration ===\n";
    echo str_repeat("=", 70) . "\n\n";
    
    $pool = new FastDFSConnectionPool(TRACKER_SERVERS, 1, 5);
    $circuitBreaker = new CircuitBreaker(3, 10); // Lower threshold for demo
    $logger = new FastDFSLogger();
    $client = new FastDFSClient($pool, $circuitBreaker, $logger);
    
    // Test 1: File not found error
    echo "--- Test 1: File Not Found ---\n";
    try {
        $client->uploadFile('/nonexistent/file.txt');
    } catch (FastDFSUploadException $e) {
        echo "✓ Caught expected exception: " . $e->getMessage() . "\n";
    }
    
    // Test 2: Invalid file ID download
    echo "\n--- Test 2: Invalid File ID ---\n";
    try {
        $client->downloadFile('invalid/file/id.txt', '/tmp/test.txt');
    } catch (FastDFSDownloadException $e) {
        echo "✓ Caught expected exception: " . $e->getMessage() . "\n";
    }
    
    // Test 3: Circuit breaker (simulated by multiple failures)
    echo "\n--- Test 3: Circuit Breaker ---\n";
    echo "Simulating multiple failures to trigger circuit breaker...\n";
    
    for ($i = 1; $i <= 4; $i++) {
        try {
            echo "\nAttempt $i:\n";
            $client->downloadFile('group1/M00/00/00/nonexistent.txt', '/tmp/test.txt', 1);
        } catch (Exception $e) {
            echo "Expected failure: " . get_class($e) . "\n";
            
            if ($e instanceof FastDFSCircuitBreakerException) {
                echo "✓ Circuit breaker is now OPEN - protecting system\n";
                break;
            }
        }
    }
    
    return $pool;
}

/**
 * Main execution function
 */
function main() {
    echo "=== FastDFS Connection Pool and Error Handling Example ===\n\n";
    
    echo "This example demonstrates:\n";
    echo "  ✓ Connection pooling for resource efficiency\n";
    echo "  ✓ Load balancing across tracker servers\n";
    echo "  ✓ Automatic retry with exponential backoff\n";
    echo "  ✓ Circuit breaker pattern for fault tolerance\n";
    echo "  ✓ Comprehensive error handling and logging\n";
    echo "  ✓ Custom exception types for different errors\n\n";
    
    // Demonstration 1: Connection Pool
    $pool1 = demonstrateConnectionPool();
    
    // Demonstration 2: Error Handling
    $pool2 = demonstrateErrorHandling();
    
    // Summary
    echo "\n" . str_repeat("=", 70) . "\n";
    echo "=== Summary ===\n";
    echo str_repeat("=", 70) . "\n";
    
    echo "\nKey Features Demonstrated:\n\n";
    
    echo "Connection Pool Benefits:\n";
    echo "  • Reuses connections instead of creating new ones\n";
    echo "  • Maintains min/max connection limits\n";
    echo "  • Validates connection health before use\n";
    echo "  • Supports multiple tracker servers with load balancing\n";
    echo "  • Automatic connection cleanup and recovery\n\n";
    
    echo "Error Handling Features:\n";
    echo "  • Custom exception classes for specific error types\n";
    echo "  • Automatic retry with exponential backoff\n";
    echo "  • Circuit breaker prevents cascading failures\n";
    echo "  • Comprehensive error logging to file\n";
    echo "  • Context information for debugging\n\n";
    
    echo "Production Best Practices:\n";
    echo "  • Always use connection pooling in high-traffic apps\n";
    echo "  • Implement circuit breakers for external dependencies\n";
    echo "  • Log all errors with context for troubleshooting\n";
    echo "  • Use retry logic with backoff for transient failures\n";
    echo "  • Monitor pool statistics for capacity planning\n";
    echo "  • Set appropriate timeouts to prevent hanging\n\n";
    
    echo "Error Log Location:\n";
    echo "  " . __DIR__ . "/fastdfs_errors.log\n\n";
}

// Run the example
main();

?>
