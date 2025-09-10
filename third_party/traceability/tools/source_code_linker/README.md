# Source Code Linker

A Bazel Rule for source code traceability for requirements. In a first step it parses the source code for requirement tags. All discovered tags including their file and line numbers are written in an intermediary file.

Carry over from Eclipse S-Core including slight modifications:
https://github.com/eclipse-score/docs-as-code/tree/v0.4.0/src/extensions/score_source_code_linker


## Implementation Components

### Bazel Integration
The extension uses two main components to integrate with Bazel:

1. `collect_source_files`
   - Processes all files from provided deps
   - Passes files as `--input` arguments to `parse_source_files.py`
   - Handles dependency tracking for incremental builds

2. `parse_source_files.py`
   - Scans input files for template tags (e.g., "#<!-- comment prevents parsing this occurance --> req-traceability:")
   - Retrieves git information (hash, file location)
   - Generates mapping file with requirement IDs and links

### Link Generation Process

1. File Discovery:
   - Takes deps from Bazel rule
   - Filters for relevant file types
   - Creates file list for processing

<br>

2. Tag Processing:
   - Scans files for template strings
   - Extracts requirement IDs
   - Maps IDs to file locations
   - *Git Integration*:
       - Gets current git hash for each file
       - Constructs GitHub URLs with format:
         `{base_url}/{repo}/blob/{hash}/{file}#L{line_nr}`
        **Note:** The base_url is defined in `parse_source_files.py`. Currently set to: `https://github.com/eclipse-score/score/blob/`

Produces JSON mapping file:
The strings are split here to not enable tracking by the source code linker.
```python
[
    {
        "file": "src/implementation1.py",
        "line": 3,
        "tag":"#" + " req-Id:",
        "need": "TREQ_ID_1",
        "full_line": "#"+" req-Id: TREQ_ID_1"
    },
    {
        "file": "src/implementation2.py",
        "line": 3,
        "tag":"#" + " req-Id:",
        "need": "TREQ_ID_1",
        "full_line": "#"+" req-Id: TREQ_ID_1"
    },
]
```
