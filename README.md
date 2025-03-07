# Dispatch

A secure, distributed file sharing and messaging system with client-server architecture.

## Overview

Dispatch is a robust system designed for secure file sharing and messaging across domains. It implements a hierarchical structure for managing users, domains, and permissions, with built-in support for public key cryptography.

## Project Structure

The project consists of two main components:

- **Client**: A user-facing application for sending and receiving files/messages
- **Server (dispatchd)**: The daemon that handles message routing and security

## Features

- **Secure Communication**: End-to-end encryption using public/private key pairs
- **Flexible Directory Structure**: Hierarchical organization of domains and users
- **Automatic Responses**: Configure automatic replies with `auto.xyz` files
- **Message Forwarding**: Set up forwarding rules with `dp.forward`
- **Mailing Lists**: Create and manage broadcast/mailing lists
- **Event Logging**: Comprehensive logging of all activities
- **File Integrity**: Checksum verification via `.index` files
- **Direct Domain Dispatch**: Send messages to domains without specifying users
- **Hook System**: TCP-based protocol for integration with other applications

## Configuration

### Daemon Configuration

The daemon is configured through `~/.dispatch/dp.conf`. This file controls:

- Network settings
- Security parameters
- Logging preferences

### Access Control

Use `~/.dispatch/dp.rules` to define:

- Whitelisted addresses
- Blacklisted addresses
- Domain-specific rules

## Security

Dispatch implements several security measures:

1. Public key cryptography for message encryption
2. Digital signatures for authenticity verification
3. Checksum verification for file integrity
4. Access control lists for domain management

## Development

The project is developed in Xcode with separate projects for client and server components:

- `client.xcodeproj`: Client application
- `dispatchd.xcodeproj`: Server daemon

## Getting Started

1. Clone the repository
2. Set up your domain configuration in the Dispatch directory
3. Configure your public/private key pairs
4. Start the dispatch daemon
5. Launch the client application

## Directory Conventions

- `.pubkey`: Contains public keys for users/servers
- `.log`: Event logging for specific directories
- `.index`: File checksums for integrity verification
- `dp.forward`: Forwarding rules configuration
- `dp.list`: Broadcast/mailing list configuration

## License

This project is licensed under the terms specified in the LICENSE file.

## Author

Created by Ali Mahouk.
