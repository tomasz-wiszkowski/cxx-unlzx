# Changes by Ender

This document summarizes the improvements and modernizations made to the unlzx project since the last commit by the original authors/maintainers.

## v1.2 Summary of Changes

*   **Reentrancy & Architecture**
    *   Made the codebase fully re-entrant by wrapping all static variables into classes.
    *   Extracted key components (`LzxEntry`, `LzxFileSegment`, `LzxBlock`) into dedicated translation units (TUs) to improve modularity.
    *   Converted the CRC module into a proper object (removing static states).

*   **Cross-Platform & Windows Support**
    *   Added full support for Windows and MSVC (including specific fixes for MSVC 2026).
    *   Integrated `gtest` fetching into the build system and re-enabled tests on Windows.
    *   Added a GitHub workflow for automated releases.

*   **Error Handling**
    *   Removed the use of exceptions in favor of using regular error codes for better control and performance.
    *   Moved error strings to a dedicated `error.cc` file.

*   **CLI & Feature Enhancements**
    *   Added the ability to filter archive contents (e.g., viewing and extracting files matching a regex pattern).
    *   Added a `view` (`-v`) option to output matching file contents directly to the console.
    *   Repurposed the `-v` flag to `-l` for listing contents.
    *   Updated the CLI to support acting on a single archive file at a time.
    *   Improved metadata reporting, including naively inferring compressed sizes from merged blocks.

*   **Code Cleanup & API Simplification**
    *   Cleaned up APIs (e.g., removed `get_` prefixes from accessors, simplified `LzxFileSegment` API).
    *   Documented public methods.
    *   Removed dead code and unused variables.
    *   Addressed compiler warnings and tightened warning levels across the project.
