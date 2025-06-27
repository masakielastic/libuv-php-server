# Aurora HTTP Server

Aurora is a high-performance HTTP server for PHP development, built with libuv for asynchronous I/O and designed as a modern replacement for PHP's built-in development server (`php -S`).

## Features

- **🚀 High Performance**: Built on libuv event loop for non-blocking I/O
- **🔒 HTTPS/TLS Support**: Full SSL/TLS encryption with certificate management
- **📁 Smart Routing**: Automatic PHP file execution and static file serving
- **🌐 Super-globals**: Automatic population of $_SERVER, $_GET, $_POST, etc.
- **⚡ Zero Configuration**: Works out of the box with sensible defaults
- **🛠 Developer Friendly**: Hot reload, verbose logging, and easy CLI interface

## Requirements

- PHP 8.4+
- libuv development libraries
- OpenSSL development libraries
- Build tools (gcc, make, autotools)

### Installation on Debian/Ubuntu

```bash
sudo apt-get update
sudo apt-get install php8.4-dev libuv1-dev libssl-dev build-essential autotools-dev
```

## Quick Start

1. **Clone and Build**
   ```bash
   git clone <repository-url>
   cd libuv_php_server
   phpize
   ./configure
   make
   ```

2. **Start HTTP Server**
   ```bash
   ./aurora serve
   ```

3. **Visit Your Application**
   ```
   http://127.0.0.1:8080/
   ```

## Usage

### Basic Commands

```bash
# Start server with defaults (127.0.0.1:8080)
./aurora serve

# Start on specific port
./aurora serve -p 3000

# Bind to all interfaces
./aurora serve -h 0.0.0.0 -p 8080

# Set document root
./aurora serve -d /var/www/html

# Enable verbose logging
./aurora serve -v
```

### HTTPS/TLS Support

```bash
# Generate self-signed certificate (for development)
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
  -days 365 -nodes -subj "/C=US/ST=Test/L=Test/O=Aurora/CN=localhost"

# Start HTTPS server
./aurora serve --tls-cert server.crt --tls-key server.key -p 8443

# Test HTTPS connection
curl -k https://127.0.0.1:8443/
```

### CLI Options

| Option | Short | Description | Default |
|---------|-------|-------------|---------|
| `--host` | `-h` | Bind address | 127.0.0.1 |
| `--port` | `-p` | Port number | 8080 |
| `--docroot` | `-d` | Document root directory | Current directory |
| `--tls-cert` | `-t` | TLS certificate file | None |
| `--tls-key` | `-k` | TLS private key file | None |
| `--verbose` | `-v` | Enable verbose output | false |
| `--help` | | Show help message | |

## Development

### Project Structure

```
├── aurora.c           # Main C extension implementation
├── php_aurora.h       # Extension header file
├── config.m4          # Build configuration
├── aurora             # CLI wrapper script
├── public/            # Example web files
│   ├── index.php      # Demo PHP page
│   └── test.html      # Static file example
├── test_serve.php     # HTTP server test
├── test_tls.php       # HTTPS server test
└── README.md          # This file
```

### Architecture

Aurora consists of three main components:

1. **C Extension** (`aurora.c`): Core HTTP server using libuv and OpenSSL
2. **PHP API**: Functions for server control and request handling
3. **CLI Wrapper** (`aurora`): User-friendly command-line interface

### Available PHP Functions

```php
// Start HTTP/HTTPS server
aurora_serve(array $options);

// Register custom request handler
aurora_register_handler(callable $handler);

// Stop the server
aurora_stop();
```

### Example PHP Usage

```php
<?php
// Start server with custom options
$options = [
    'host' => '127.0.0.1',
    'port' => 8080,
    'docroot' => __DIR__ . '/public',
    'ssl' => true,
    'ssl_cert' => 'server.crt',
    'ssl_key' => 'server.key'
];

// Register custom handler (optional)
aurora_register_handler(function($env, $respond) {
    $respond(200, ['Content-Type' => 'application/json'], 
             json_encode(['message' => 'Hello from Aurora!']));
});

// Start server
aurora_serve($options);
?>
```

## Testing

### Run Test Suite

```bash
# Test basic HTTP functionality
php test_serve.php

# Test HTTPS/TLS functionality
php test_tls.php

# Test with curl
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/test.html
curl -k https://127.0.0.1:8443/
```

### Example Files

The `public/` directory contains example files:

- `index.php`: Dynamic PHP page with server information
- `test.html`: Static HTML file with CSS styling

## Configuration

### Environment Variables

```bash
# Extension loading
export PHP_INI_SCAN_DIR="/path/to/aurora/modules"

# Development mode
export AURORA_DEBUG=1
```

### PHP Configuration

Aurora can be loaded as a PHP extension:

```ini
; php.ini
extension=/path/to/aurora.so
```

## Performance

Aurora is designed for development use and provides:

- Non-blocking I/O with libuv event loop
- Efficient HTTP parsing with llhttp
- Zero-copy static file serving
- Minimal memory footprint
- Fast SSL/TLS handshake

## Troubleshooting

### Common Issues

1. **Extension not found**
   ```bash
   # Build the extension first
   make clean && make
   ```

2. **Permission denied on port**
   ```bash
   # Use unprivileged port (>1024)
   ./aurora serve -p 8080
   ```

3. **SSL certificate errors**
   ```bash
   # Generate new certificate
   openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes
   ```

4. **PHP file not executing**
   ```bash
   # Check file permissions and document root
   ./aurora serve -d /path/to/your/files -v
   ```

### Debug Mode

Enable verbose output to see detailed server operations:

```bash
./aurora serve -v
```

## Roadmap

- [ ] HTTP/2 support with libh2o integration
- [ ] WebSocket support
- [ ] Built-in PHP debugger integration
- [ ] Performance monitoring and metrics
- [ ] Docker container support
- [ ] Systemd service integration

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## License

This project is licensed under the MIT License. See LICENSE file for details.

## Acknowledgments

- Built with [libuv](https://libuv.org/) for async I/O
- HTTP parsing by [llhttp](https://github.com/nodejs/llhttp)
- SSL/TLS support via [OpenSSL](https://www.openssl.org/)
- Inspired by PHP's built-in development server

## Support

For questions, issues, or contributions:

- Create an issue on GitHub
- Check the troubleshooting section
- Review the example files in `public/`

---

**Aurora HTTP Server** - Fast, secure, and developer-friendly PHP development server.