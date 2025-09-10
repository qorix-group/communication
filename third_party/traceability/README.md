# Tooling for Traceability Concept

!!THIS CONCEPT IS CURRENTLY IN A POC STAGE!!

In this document the tooling for the platform traceability concept is described. It basically consists of 4 core Items:

1. Requirement Definition (TRLC)
2. Linking Requirements to Source (source_code_linker)
3. Linking Requirements to Tests (gtest, python)
4. Displaying Traceability

An and additional item for the visualization of the requirements in Sphinx Needs:

5. TRLC Renderer

## Folder Structure

The Traceability Tooling is divided into configuration, tools and bazel rules.

The Tools TRLC and Lobster - which are also required by the traceability tooling - are python packages and are therefore configured
via third_party

## Requirements Definition

### Metamodel
The Requirements Definition is done in TRLC, therefore folowing metamodel (.rsl file) was defined:

```
Requirement (abstract)
│
├── AssumedSystemReq (extends Requirement)
│   └── AssumedSystemReqId (tuple)
│
├── FeatReq (extends Requirement)
│   └── FeatReqId (tuple)
│       └── derived_from: AssumedSystemReqId[1..*]
│
└── CompReq (extends Requirement)
    └── derived_from: FeatReqId[1..*]
```

It consists of 4 Types:
- Abstract Requirements Type
- Assumed System Requirement
- Feature Requirement
- Component Requirement

A more detailes view including all attributes is displayed here:

```
Requirement (abstract)
│   ├── description: String
│   ├── version: Integer
│   ├── safety: Asil
│   ├── note (optional): String
│   ├── status: Status
│   └── freeze status = Status.valid
│
├── AssumedSystemReq (extends Requirement)
│   ├── rationale: String
│   └── AssumedSystemReqId (tuple)
│       ├── item: AssumedSystemReq
│       ├── separator: @
│       └── LinkVersion: Integer
│
├── FeatReq (extends Requirement)
│   ├── derived_from: AssumedSystemReqId[1 .. *]
│   └── FeatReqId (tuple)
│       ├── item: FeatReq
│       ├── separator: @
│       └── LinkVersion: Integer
│
└── CompReq (extends Requirement)
    ├── derived_from: FeatReqId[1 .. *]
```

### Definition of Requirements

For the definition of requirements a trlc file needs to be generated. The basic concept is described here:

https://github.com/score-software-engineering/trlc/blob/main/documentation/TUTORIAL-BASIC.md

Additionally a Plugin for VScode is available:

https://github.com/score-software-engineering/trlc-vscode-extension/releases

In addition to the definition of the requirements in the .trlc file Bazel targets needs to be created:

```bazel
trlc_requirements(
    name = "...",
    srcs = [
        "... .trlc",
        "another trlc_requirements target"
    ],
    spec = [
        trlc_specification target",
    ],
    visibility = ["//visibility:public"],
)
```

```Bazel
trlc_requirements_test(
    name = "...",
    reqs = [
        ":trlc_requirements target",
    ],
    visibility = ["//visibility:public"],
)
```

For testing the reqirements a `bazel test` command can be run on the `trlc_requirements_test` target

### Linking requirements to Source Code

For linking requirements to source code only a comment needs to be added to the cpp file:

```cpp
// trace: <trlc id>
```

Then it will be automatically available in the lobster traceability report

### Linking requirements to Test cases

#### CPP
Google test cases can be linked via an additional *record_property*:

```cpp
RecordProperty("lobster-tracing", "<TRLC Id>");
```
### Traceability

After linking all requirements to the defined levels and to code and implementation a traceability report can be generated via bazel test, e.g:

```
bazel test //platform/aas/mw/com:traceability --config=spp_host_gcc
```
