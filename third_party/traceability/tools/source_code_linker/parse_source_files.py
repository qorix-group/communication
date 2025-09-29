# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

## Carry over from Eclipse S-Core including slight modifications:
# https://github.com/eclipse-score/docs-as-code/tree/v0.4.0/src/extensions/score_source_code_linker

import argparse
import collections
import json
import logging
import sys
import os
from typing import Callable, Dict, List, Optional, Tuple, Union

# Add current directory to Python path for imports
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from git_utils import get_github_repo, get_git_hash

logger = logging.getLogger(__name__)

TAGS_STD = [
    "# trace:",
    "// trace:",
    "' trace:",
]

lobster_code_template = {
        "data": [],
        "generator": "lobster_cpp",
        "schema": "lobster-imp-trace",
        "version": 3
    }

lobster_reqs_template = {
        "data": [],
        "generator": "lobster-trlc",
        "schema": "lobster-req-trace",
        "version": 4
    }

def extract_id_from_line_standard(line: str, tags: List[str]) -> Optional[str]:
    """
    Parse a single line to extract the ID from standard tags (e.g., // trace:, # trace:).

    Steps:
    1. Clean the line by removing '\n' and '\\n'
    2. Remove all single and double quotes
    3. Search for any tag
    4. Capture the ID from the tag

    Parameters
    ----------
    line : str
        A single line of text containing a tag.
    tags : List[str]
        A list of tag strings to search for.

    Returns
    -------
    Optional[str]
        The extracted ID if found, None otherwise.

    Examples
    --------
    >>> tags = ["# trace:", "// trace:", "' trace:"]
    >>> extract_id_from_line_standard('# trace: id123', tags)
    'id123'
    >>> extract_id_from_line_standard('// trace: id456', tags)
    'id456'
    >>> extract_id_from_line_standard("Invalid line", tags)
    None
    """
    # Step 1: Clean the line of \n, and \\n
    cleaned_line = line.replace('\n', '').replace('\\n', '')

    # Step 2: Remove all single and double quotes
    cleaned_line = cleaned_line.replace('"', '').replace("'", '').replace("{",'').strip()

    # Step 3 and 4: Search for tags and capture the ID
    for tag in tags:
        if cleaned_line.startswith(tag):
            return cleaned_line.split(tag, 1)[1].strip()

    return None


def extract_id_from_line_plantuml(line: str, nodes: List[str]) -> Optional[str]:
    """
    Parse a single line to extract the ID from PlantUML nodes (e.g., $TopEvent, $BasicEvent).

    Parameters
    ----------
    line : str
        A single line of text containing a PlantUML node.
    nodes : List[str]
        A list of node strings to search for.

    Returns
    -------
    Optional[str]
        The extracted ID if found, None otherwise.

    Examples
    --------
    >>> nodes = ["$TopEvent", "$BasicEvent"]
    >>> extract_id_from_line_plantuml('$TopEvent(Description, id456)', nodes)
    'id456'
    >>> extract_id_from_line_plantuml('$BasicEvent(Description,id789,extra1,extra2)', nodes)
    'id789'
    >>> extract_id_from_line_plantuml("Invalid line", nodes)
    None
    """
    # Step 1: Clean the line of \n, and \\n
    cleaned_line = line.replace('\n', '').replace('\\n', '')

    # Step 2: Remove all single and double quotes
    cleaned_line = cleaned_line.replace('"', '').replace("'", '').replace("{",'').strip()

    # Step 3: Search for nodes and capture the ID
    for node in nodes:
        if cleaned_line.startswith(node):
            # Scan for Macro
            if node.startswith('$'):
                parts = cleaned_line.split(',')
                if len(parts) >= 2:
                    return parts[1].strip().rstrip(')')
            else: # scan for normal plantuml element
                # Remove the identifier from the start of the line
                parts = cleaned_line[len(node):].strip().split()

                # If there are at least 3 parts and the second-to-last is 'as', return the last part
                if len(parts) >= 3 and parts[-2] == 'as':
                    return parts[-1], node

                # If there's only one part after the identifier, return it
                if len(parts) == 1:
                    return parts[0]

    return None


def extract_interface_methods_from_line(line: str, interface_name: str = None) -> Optional[str]:
    """
    Parse a single line to extract interface method definitions from PlantUML.

    Looks for patterns like:
    - +get_requirements() : List<Requirement>
    - +get_hazard_analysis() : List<Hazard>

    Returns the method name in the format: InterfaceName.method_name

    Parameters
    ----------
    line : str
        A single line of text containing a method definition.
    interface_name : str
        The name of the interface (extracted from previous lines).

    Returns
    -------
    Optional[str]
        The extracted method ID if found, None otherwise.
    """
    # Step 1: Clean the line
    cleaned_line = line.replace('\n', '').replace('\\n', '').strip()

    # Step 2: Look for method definitions starting with + or -
    if cleaned_line.startswith(('+', '-')):
        # Remove the visibility modifier
        method_part = cleaned_line[1:].strip()

        # Extract method name (everything before the opening parenthesis)
        if '(' in method_part:
            method_name = method_part.split('(')[0].strip()

            # Create the full method identifier
            if interface_name and method_name:
                return f"{interface_name}.{method_name}"
            elif method_name:
                return f"Interface.{method_name}"  # Fallback if no interface name

    return None


def extract_interface_name_from_line(line: str) -> Optional[str]:
    """
    Extract interface name from PlantUML interface definition lines.

    Looks for patterns like:
    - interface "Public API" as PublicAPI {
    - interface PublicAPI {

    Returns the interface alias name (e.g., "PublicAPI").
    """
    cleaned_line = line.replace('\n', '').replace('\\n', '').strip()

    if cleaned_line.startswith('interface'):
        # Remove 'interface' keyword
        rest = cleaned_line[9:].strip()

        # Look for 'as InterfaceName' pattern
        if ' as ' in rest:
            parts = rest.split(' as ')
            if len(parts) >= 2:
                # Extract name before the opening brace
                interface_name = parts[1].split('{')[0].strip()
                return interface_name

        # Fallback: look for direct interface name
        if '{' in rest:
            interface_name = rest.split('{')[0].strip()
            # Remove quotes if present
            interface_name = interface_name.strip('"\'')
            return interface_name

    return None


def extract_tags_standard(
    source_file: str,
    github_base_url: str,
    tags: List[str],
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[str]]:
    """
    Extract tags from standard source files (non-PlantUML files).

    Args:
        source_file (str): path to source file that should be parsed.
        tags (List[str]): list of tag strings to search for.
        git_hash_func (Optional[callable]): Optional parameter for testing.
    """
    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    requirement_mapping: Dict[str, List[str]] = collections.defaultdict(list)
    with open(source_file) as f:
        hash = git_hash_func(source_file)
        for line_number, line in enumerate(f):
            line_number = line_number + 1
            result = extract_id_from_line_standard(line, tags)
            if result is None:
                continue

            req_id = result

            if req_id:
                link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                requirement_mapping[req_id].append(link)

    return requirement_mapping


def extract_tags_plantuml(
    source_file: str,
    github_base_url: str,
    nodes: List[str],
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[str]]:
    """
    Extract tags from PlantUML files (.puml files).

    Supports two types of parsing:
    1. Macro-based elements (e.g., $TopEvent, $BasicEvent)
    2. Interface method definitions (e.g., +get_requirements())

    Args:
        source_file (str): path to PlantUML file that should be parsed.
        nodes (List[str]): list of PlantUML node strings to search for.
        git_hash_func (Optional[callable]): Optional parameter for testing.
    """
    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    requirement_mapping: Dict[str, List[str]] = collections.defaultdict(list)
    current_interface_name = None

    with open(source_file) as f:
        hash = git_hash_func(source_file)
        for line_number, line in enumerate(f):
            line_number = line_number + 1

            # Check for interface definition
            interface_name = extract_interface_name_from_line(line)
            if interface_name:
                current_interface_name = interface_name
                continue

            # Check for macro-based elements (existing functionality)
            macro_result = extract_id_from_line_plantuml(line, nodes)
            if macro_result is not None:
                req_id = macro_result
                if req_id:
                    link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                    requirement_mapping[req_id].append(link)
                continue

            # Check for interface method definitions (new functionality)
            method_result = extract_interface_methods_from_line(line, current_interface_name)
            if method_result is not None:
                req_id = method_result
                if req_id:
                    link = f"{github_base_url}/blob/{hash}/{source_file}#L{line_number}"
                    requirement_mapping[req_id].append(link)
                continue

    return requirement_mapping


def extract_tags(
    source_file: str,
    github_base_url: str,
    tags: List[str],
    nodes: List[str] = None,
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[str]]:
    """
    Dispatch to appropriate parser based on file extension.

    Args:
        source_file (str): path to source file that should be parsed.
        tags (List[str]): list of tag strings to search for.
        nodes (List[str]): list of PlantUML node strings to search for (for .puml files).
        git_hash_func (Optional[callable]): Optional parameter for testing.
    """
    if source_file.endswith('.puml'):
        # Use PlantUML parser - separate PlantUML nodes from regular tags
        plantuml_nodes = [tag for tag in tags if tag.startswith('$')]

        # Check if interface parsing is requested (using special tag "interface")
        parse_interfaces = "interface" in tags or any(tag.lower() == "interface" for tag in tags)

        if plantuml_nodes or parse_interfaces:
            return extract_tags_plantuml(source_file, github_base_url, plantuml_nodes, git_hash_func)
        else:
            # No PlantUML nodes or interface parsing requested, return empty result
            return {}
    else:
        # Use standard tag parser - filter out PlantUML nodes for standard files
        standard_tags = [tag for tag in tags if not tag.startswith('$')]
        if not standard_tags:
            # No standard tags provided, return empty result
            return {}
        return extract_tags_standard(source_file, github_base_url, standard_tags, git_hash_func)

def _extract_tags_dispatch(
    source_file: str,
    github_base_url: str,
    mode: str,
    tags: List[str],
    nodes: List[str] = None,
    git_hash_func: Union[Callable[[str], str], None] = get_git_hash
) -> Dict[str, List[str]]:
    """
    Extract tags from source file and format them according to the specified mode.
    TAGS are now independent of mode - the same tags are used for both reqs and code.
    Automatically detects file type and uses appropriate parser.
    """
    # force None to get_git_hash
    if git_hash_func is None:
        git_hash_func = get_git_hash

    # Extract tags using appropriate parser based on file extension
    rm = extract_tags(source_file, github_base_url, tags, nodes, git_hash_func)

    if mode == "reqs":
        foo = lobster_reqs_template.copy()
        for id, links in rm.items():
            link = links[0]  # Take first link if multiple

            requirement = {
                "tag": f"req {id}",
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "framework": "TRLC",
                "kind": "requirement",
                "text": f"{id}",
                }

            foo["data"].append(requirement)
    elif mode == "interface":
        # Mode is "interface" - generate interface method requirements
        foo = lobster_reqs_template.copy()
        for id, links in rm.items():
            link = links[0]  # Take first link if multiple

            interface_req = {
                "tag": f"req {id}",
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "framework": "TRLC",
                "kind": "requirement",
                "text": f"{id}",
            }

            foo["data"].append(interface_req)
    else:
        # Mode is "code"
        foo = lobster_code_template.copy()
        for id, links in rm.items():
            link = links[0]  # Take first link if multiple
            # Extract line number from URL (e.g., "file.cpp#L6" -> "6")
            line_number = "0"
            if "#L" in link:
                line_number = link.split("#L")[1]
            # Create unique tag with file and line number
            unique_tag = f"cpp {id}:{source_file}:{line_number}"

            codetag = {
                "tag": unique_tag,
                "location": {
                    "kind": "file",
                    "file": link.strip(),
                    "line": 1,
                    "column": 1,
                },
                "name": f"{source_file.strip()}",
                "messages": [],
                "just_up": [],
                "just_down": [],
                "just_global": [],
                "refs": [
                    f"req {id}"
                ],
                "language": "cpp",
                "kind": "Function"
                }
            foo["data"].append(codetag)

    return foo

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", "--output")
    parser.add_argument("-t", "--trace", default="code")
    parser.add_argument("-u", "--url", default="https://www.github.com/")
    parser.add_argument("--tags")
    parser.add_argument("inputs", nargs="*")

    args, _ = parser.parse_known_args()

    # Process optional tag overrides
    current_tags = TAGS_STD.copy()
    if args.tags:
        extra_tags = [t.strip() for t in args.tags.split("|") if t.strip()]
        current_tags.extend(extra_tags)

    logger.info(f"Parsing source files: {args.inputs}")
    logger.info(f"Using tags: {current_tags}")

    # Finding the GH URL
    gh_base_url = f"{args.url}{get_github_repo()}"

    # Process all input files
    all_results = {"data": [], "generator": "", "schema": "", "version": 0}
    processed_files = set()  # Track processed files to avoid processing same file multiple times

    for input_file in args.inputs:
        with open(input_file) as f:
            for source_file in f:
                source_file = source_file.strip()

                # Only prevent processing the exact same file multiple times in the same run
                # This allows multiple tags from the same file (at different locations)
                # but prevents duplicate processing of the same file
                if source_file in processed_files:
                    logger.debug(f"Skipping already processed file: {source_file}")
                    continue

                processed_files.add(source_file)
                logger.debug(f"Processing file: {source_file}")

                result = _extract_tags_dispatch(
                    source_file,
                    gh_base_url,
                    args.trace,
                    current_tags,
                    current_tags  # Use the same tags for both standard and PlantUML files
                )
                # Merge results - take metadata from first result, accumulate data
                if not all_results["generator"]:
                    all_results.update({k: v for k, v in result.items() if k != "data"})
                all_results["data"].extend(result["data"])

    # Deduplicate entries that are truly identical (same tag, same file, same location)
    # but allow multiple entries for the same tag at different locations
    seen_entries = set()
    deduped_data = []
    for item in all_results["data"]:
        # Create a unique key based on tag, file, and location (line number from URL)
        tag = item.get("tag", "")
        file_url = item.get("location", {}).get("file", "")
        # The line number is in the URL as #L<number>
        entry_key = (tag, file_url)

        if entry_key not in seen_entries:
            seen_entries.add(entry_key)
            deduped_data.append(item)
        else:
            logger.debug(f"Removing truly duplicate entry: {tag} at {file_url}")

    all_results["data"] = deduped_data

    with open(args.output, "w") as f:
        f.write(json.dumps(all_results, indent=2))
