# YouTubeCacher v0.0.0 - Initial Release

## Overview

YouTubeCacher is a Windows GUI application for downloading and managing YouTube videos with a focus on caching, organization, and offline viewing. This initial release provides core functionality for video downloading, cache management, and a modern Windows interface.

## Key Features

### 🎥 Video Download System
- **YouTube URL Support**: Download videos from standard YouTube URLs, youtu.be short links, and YouTube Shorts
- **yt-dlp Integration**: Powered by yt-dlp for reliable video extraction and downloading
- **Quality Selection**: Automatic best quality selection with fallback options
- **Subtitle Support**: Automatic download of available subtitles in multiple formats (SRT, VTT, ASS, SSA, SUB)
- **Progress Tracking**: Real-time download progress with detailed status updates

### 💾 Advanced Cache Management
- **Bulletproof Cache System**: Robust cache index with comprehensive error handling and recovery
- **File Validation**: Automatic validation of downloaded files and cache integrity
- **Metadata Storage**: Persistent storage of video titles, durations, file paths, and subtitle information
- **Cache Scanning**: Automatic detection of existing videos in download folder
- **Thread-Safe Operations**: Multi-threaded cache operations with proper synchronization

### 🖥️ Modern Windows Interface
- **Native Windows GUI**: Clean, modern interface following Windows design guidelines
- **Resizable Layout**: Adaptive UI that scales properly with window resizing
- **ListView Integration**: Organized display of cached videos with sortable columns
- **Color-Coded Input**: Visual feedback for different URL input sources (manual, clipboard, autopaste)
- **Settings Dialog**: Comprehensive configuration options for all application features

### 🔧 Configuration & Settings
- **Registry Integration**: Persistent settings storage in Windows registry
- **Download Path Management**: Configurable download directory with automatic creation
- **yt-dlp Path Configuration**: Flexible yt-dlp executable location settings
- **Custom Arguments**: Support for custom yt-dlp command-line arguments
- **Debug & Logging**: Comprehensive logging system with configurable debug output

### 📋 Clipboard Integration
- **Auto-paste Detection**: Automatic detection and processing of YouTube URLs from clipboard
- **Manual Paste Support**: Traditional Ctrl+V paste functionality
- **URL Validation**: Real-time validation of YouTube URL formats
- **Smart URL Processing**: Handles various YouTube URL formats and parameters

### 🎮 Media Player Integration
- **External Player Support**: Launch cached videos in configured media player (VLC, etc.)
- **File Association**: Direct integration with Windows file associations
- **Subtitle Handling**: Automatic subtitle file detection and loading

## Technical Architecture

### 🏗️ Modular Design
- **Separation of Concerns**: Clean separation between UI, cache, download, and settings modules
- **Header Organization**: Centralized constants and configuration in master header
- **Resource Management**: Proper Windows resource handling and cleanup

### 🔒 Thread Safety
- **Critical Sections**: Thread-safe cache operations and state management
- **Background Downloads**: Non-blocking download operations with progress callbacks
- **UI Thread Safety**: Proper UI updates from background threads

### 🛡️ Error Handling
- **Comprehensive Validation**: Input validation, file system checks, and error recovery
- **Graceful Degradation**: Application continues functioning even with partial failures
- **Detailed Logging**: Extensive debug output for troubleshooting and diagnostics

### 🌐 Unicode Support
- **Full Unicode Compliance**: UTF-16 APIs throughout for international character support
- **Modern Windows APIs**: Exclusive use of Unicode Windows API functions
- **Proper Text Encoding**: UTF-8 file handling with fallback support

## System Requirements

- **Operating System**: Windows 10 or later (Windows 11 recommended)
- **Architecture**: 32-bit and 64-bit support
- **Dependencies**: 
  - yt-dlp (automatically detected or manually configured)
  - Windows Media Foundation (for video file handling)
  - Visual C++ Redistributable (statically linked)

## Installation

1. Download the appropriate executable for your system architecture
2. Run the application - no installation required (portable)
3. Configure download path and yt-dlp location in Settings
4. Start downloading and caching YouTube videos

## Configuration

### First Run Setup
1. **Download Folder**: Set your preferred download directory (defaults to `Downloads\YouTubeCacher`)
2. **yt-dlp Path**: Configure path to yt-dlp executable (auto-detected if in PATH)
3. **Media Player**: Set preferred video player for cached content playback
4. **Logging**: Enable debug logging for troubleshooting if needed

### Advanced Options
- **Custom yt-dlp Arguments**: Add specialized download parameters
- **Auto-paste Settings**: Configure clipboard monitoring behavior
- **Debug Output**: Enable detailed logging for development and troubleshooting

## Known Limitations

- **Single Download**: Currently supports one download at a time
- **Basic Playlist Support**: Individual video downloads only (no playlist batch processing)
- **Windows Only**: Native Windows application, not cross-platform

## Development Notes

### Build System
- **MSYS2/MinGW**: Built with MinGW-w64 toolchain
- **Static Linking**: Self-contained executable with minimal dependencies
- **Resource Compilation**: Integrated Windows resource compilation

### Code Quality
- **Strict Compilation**: Built with `-Wall -Wextra -Werror` for maximum code quality
- **Memory Safety**: Comprehensive memory management and leak prevention
- **Error Handling**: Defensive programming with extensive error checking

## Future Roadmap

- **Playlist Support**: Batch downloading of YouTube playlists
- **Download Queue**: Multiple simultaneous downloads with queue management
- **Video Conversion**: Built-in format conversion capabilities
- **Search Integration**: Search and browse cached videos
- **Export/Import**: Cache backup and restore functionality

## Support

For issues, feature requests, or contributions, please visit the project repository.

---

**YouTubeCacher v0.0.0** - A robust, modern solution for YouTube video caching and management on Windows.