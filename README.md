# uvhttp Server

uvhttp is a high-performance HTTP server for PHP development, built with libuv for asynchronous I/O and designed as a modern replacement for PHP's built-in development server (`php -S`).

## Features

- **🚀 High Performance**: Built on libuv event loop for non-blocking I/O
- **🔒 HTTPS/TLS Support**: Full SSL/TLS encryption with certificate management
- **📁 Smart Routing**: Automatic PHP file execution and static file serving
- **🌐 Super-globals**: Automatic population of $_SERVER, $_GET, $_POST, etc.
- **⚡ Zero Configuration**: Works out of the box with sensible defaults
- **🛠 Developer Friendly**: Easy CLI interface

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
   cd libuv-php-server
   phpize
   ./configure --enable-uvhttp
   make
   ```

2. **Start HTTP Server**
   ```bash
   php bin/uvhttp-server.php --host=127.0.0.1 --port=8080 --docroot=public
   ```

3. **Visit Your Application**
   ```
   http://127.0.0.1:8080/
   ```

## Usage

### Basic Commands

```bash
# Start server with defaults (127.0.0.1:8080)
php bin/uvhttp-server.php

# Start on specific port
php bin/uvhttp-server.php --port=3000

# Bind to all interfaces
php bin/uvhttp-server.php --host=0.0.0.0 --port=8080

# Set document root
php bin/uvhttp-server.php --docroot=/var/www/html
```

### HTTPS/TLS Support

```bash
# Generate self-signed certificate (for development)
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt \
  -days 365 -nodes -subj "/C=US/ST=Test/L=Test/O=uvhttp/CN=localhost"

# Start HTTPS server
php bin/uvhttp-server.php --port=8443 --docroot=public --tls-cert=server.crt --tls-key=server.key

# Test HTTPS connection
curl -k https://127.0.0.1:8443/
```

## Development

### Project Structure

```
├── uvhttp.c           # Main C extension implementation
├─
 php_uvhttp.h       # Extension header file
├── config.m4          # Build configuration
├── libuvhttp/         # Core C library
├── public/            # Example web files
│   ├── index.php      # Demo PHP page
│   └── test.html      # Static file example
└── README.md          # This file
```

### Architecture

uvhttp consists of two main components:

1. **C Extension** (`uvhttp.c`): The glue between PHP and the C library.
2. **Core Library** (`libuvhttp`): Core HTTP server using libuv, llhttp and OpenSSL.

### Available PHP Functions

```php
// Start HTTP/HTTPS server
uvhttp_serve(array $options);

// Stop the server
uvhttp_stop();
```

## Testing

### Run Test Suite

```bash
# Test basic HTTP functionality
php tests/server_test.php

# Test HTTPS/TLS functionality
php tests/test_tls.php

# Test with curl
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/test.html
curl -k https://127.0.0.1:8443/
```

## License

This project is licensed under the MIT License.

## Acknowledgments

- Built with [libuv](https://libuv.org/) for async I/O
- HTTP parsing by [llhttp](https://github.com/nodejs/llhttp)
- SSL/TLS support via [OpenSSL](https://www.openssl.org/)
- Inspired by PHP's built-in development server

---

**uvhttp Server** - Fast, secure, and developer-friendly PHP development server.
