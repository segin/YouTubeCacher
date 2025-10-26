# YouTubeCacher

A Windows GUI application for downloading YouTube videos with basic caching functionality.

![Windows](https://img.shields.io/badge/Windows-10%2B-blue?logo=windows)
![License](https://img.shields.io/badge/License-MIT-green)
![Version](https://img.shields.io/badge/Version-0.0.1-orange)

## Overview

YouTubeCacher is a simple Windows desktop application that provides a graphical interface for downloading YouTube videos using yt-dlp. It includes basic caching to track downloaded videos and avoid duplicates.

**Note**: This is version 0.0.1 - early development release with core functionality.

## Current Features

- **YouTube URL Support**: Download videos from YouTube URLs and youtu.be short links
- **yt-dlp Integration**: Uses yt-dlp as the backend for video downloading
- **Basic Cache System**: Tracks downloaded videos to prevent duplicates
- **Windows GUI**: Native Windows interface with resizable layout
- **Settings Management**: Configure download paths and yt-dlp location
- **URL Validation**: Validates YouTube URLs before processing
- **Unicode Support**: Full Unicode support for international characters

## System Requirements

- Windows 10 or later
- yt-dlp executable (can be configured in settings)

## Installation

1. Download `YouTubeCacher.exe` from the releases
2. Run the executable (no installation required)
3. Configure your download folder and yt-dlp path in Settings menu
4. Start downloading videos

## Usage

1. **First Run**: Go to Settings to configure your download folder and yt-dlp path
2. **Download Videos**: Paste a YouTube URL in the input field and click Download
3. **View Cache**: Downloaded videos are tracked in the cache list
4. **Settings**: Access File > Settings to modify configuration

## Building from Source

Requires MSYS2/MinGW development environment:

```bash
git clone https://github.com/segin/YouTubeCacher.git
cd YouTubeCacher
make clean && make
```

## Current Limitations

- Single download at a time
- No playlist support
- No subtitle downloading
- Basic cache management only
- Windows only

## Planned Features

- **Subtitle Support**: Download subtitles in multiple formats
- **Playlist Support**: Batch download from playlists
- **Progress Tracking**: Real-time download progress display
- **Media Player Integration**: Launch videos in external players
- **Advanced Cache Management**: Better organization and search
- **Auto-paste Detection**: Automatic clipboard URL detection
- **Download Queue**: Multiple simultaneous downloads

## Contributing

This project is in early development. Contributions welcome:

- Report bugs via GitHub Issues
- Submit feature requests
- Contribute code improvements
- Help with testing

## License

MIT License - see [LICENSE](LICENSE) file for details.