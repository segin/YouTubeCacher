OutputLineType ClassifyOutputLine(const wchar_t* line) {
    if (!line) return LINE_TYPE_UNKNOWN;

    // Convert to lowercase for pattern matching
    wchar_t* lowerLine = SAFE_WCSDUP(line);
    if (!lowerLine) return LINE_TYPE_UNKNOWN;

    for (wchar_t* p = lowerLine; *p; p++) {
        *p = towlower(*p);
    }

    OutputLineType result = LINE_TYPE_UNKNOWN;

    // Check for various patterns
    if (wcsstr(lowerLine, L"[info]") && wcsstr(lowerLine, L"extracting")) {
        result = LINE_TYPE_INFO_EXTRACTION;
    }
    else if (wcsstr(lowerLine, L"[info]") && (wcsstr(lowerLine, L"format") || wcsstr(lowerLine, L"quality"))) {
        result = LINE_TYPE_FORMAT_SELECTION;
    }
    else if (wcsstr(lowerLine, L"[download]") && (wcsstr(lowerLine, L"%") || wcsstr(lowerLine, L"downloading"))) {
        result = LINE_TYPE_DOWNLOAD_PROGRESS;
    }
    // Check for pipe-delimited progress format: numbers|numbers|numbers|numbers
    else if (wcschr(line, L'|')) {
        // Simple heuristic: if line has pipes and starts with digits, likely progress
        const wchar_t* p = line;
        while (*p && iswspace(*p)) p++; // Skip leading whitespace
        if (*p && iswdigit(*p)) {
            // Count pipes to see if it matches expected format (3 pipes = 4 fields)
            int pipeCount = 0;
            for (const wchar_t* q = line; *q; q++) {
                if (*q == L'|') pipeCount++;
            }
            if (pipeCount >= 3) { // At least 3 pipes for downloaded|total|speed|eta format
                result = LINE_TYPE_DOWNLOAD_PROGRESS;
            }
        }
    }
    else if (wcsstr(lowerLine, L"[ffmpeg]") || wcsstr(lowerLine, L"post-process") || wcsstr(lowerLine, L"converting")) {
        result = LINE_TYPE_POST_PROCESSING;
    }
    else if (wcsstr(lowerLine, L"destination:") || wcsstr(lowerLine, L"saving to:") ||
             (wcsstr(lowerLine, L"[download]") && wcsstr(lowerLine, L"has already been downloaded"))) {
        result = LINE_TYPE_FILE_DESTINATION;
    }
    else if (wcsstr(lowerLine, L"error") || wcsstr(lowerLine, L"failed") || wcsstr(lowerLine, L"exception")) {
        result = LINE_TYPE_ERROR;
    }
    else if (wcsstr(lowerLine, L"warning") || wcsstr(lowerLine, L"warn")) {
        result = LINE_TYPE_WARNING;
    }
    else if (wcsstr(lowerLine, L"100%") || wcsstr(lowerLine, L"download completed") ||
             wcsstr(lowerLine, L"finished downloading")) {
        result = LINE_TYPE_COMPLETION;
    }
    else if (wcsstr(lowerLine, L"[debug]")) {
        result = LINE_TYPE_DEBUG;
    }

    SAFE_FREE(lowerLine);
    return result;
}
