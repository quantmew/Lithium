# Lithium Browser Engine - Architecture

## Overview

Lithium is a lightweight browser engine implemented from scratch in C++20. It provides complete HTML parsing, CSS styling, JavaScript execution, and rendering capabilities.

## Core Design Principles

1. **Modularity**: Each component is self-contained with clear interfaces
2. **Standards Compliance**: Follow WHATWG HTML, CSS, and ECMAScript specifications
3. **Performance**: Efficient memory usage and fast rendering
4. **Cross-Platform**: Works on Windows, macOS, and Linux

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Browser Shell                             │
│                    (UI, Tabs, Navigation)                        │
└───────────────────────────┬─────────────────────────────────────┘
                            │
┌───────────────────────────┴─────────────────────────────────────┐
│                      Browser Engine                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐     │
│  │  Network │   │   HTML   │   │   CSS    │   │    JS    │     │
│  │  Module  │   │  Parser  │   │  Parser  │   │  Engine  │     │
│  └────┬─────┘   └────┬─────┘   └────┬─────┘   └────┬─────┘     │
│       │              │              │              │            │
│       │              ▼              ▼              │            │
│       │         ┌─────────────────────────┐       │            │
│       │         │          DOM            │◄──────┘            │
│       │         │   (Document Object      │                    │
│       └────────►│        Model)           │                    │
│                 └───────────┬─────────────┘                    │
│                             │                                   │
│                             ▼                                   │
│                 ┌─────────────────────────┐                    │
│                 │    Style Resolution     │                    │
│                 │  (CSS Cascade + CSSOM)  │                    │
│                 └───────────┬─────────────┘                    │
│                             │                                   │
│                             ▼                                   │
│                 ┌─────────────────────────┐                    │
│                 │      Layout Engine      │                    │
│                 │   (Box Model, Flow)     │                    │
│                 └───────────┬─────────────┘                    │
│                             │                                   │
│                             ▼                                   │
│                 ┌─────────────────────────┐                    │
│                 │    Rendering Pipeline   │                    │
│                 │  (Paint, Composite)     │                    │
│                 └───────────┬─────────────┘                    │
│                             │                                   │
└─────────────────────────────┼───────────────────────────────────┘
                              │
┌─────────────────────────────┴───────────────────────────────────┐
│                     Platform Abstraction                         │
│              (Window, Graphics, Input, Fonts)                    │
└─────────────────────────────────────────────────────────────────┘
```

## Module Descriptions

### Core (`src/core/`)

Foundation types and utilities used throughout the codebase.

- **types.hpp**: Basic types (Result, RefPtr, Point, Rect, Color)
- **string.hpp**: UTF-8 string with Unicode support
- **memory.hpp**: Arena allocator, memory spans
- **logger.hpp**: Logging infrastructure

### Platform (`src/platform/`)

Platform abstraction layer for cross-platform support.

- **window.hpp**: Window creation and management
- **event.hpp**: Input events (keyboard, mouse)
- **graphics_context.hpp**: 2D drawing primitives

Implementations:
- `src/platform/windows/` - Win32 API
- `src/platform/macos/` - Cocoa/AppKit
- `src/platform/linux/` - X11/Wayland

### DOM (`src/dom/`)

Document Object Model implementation.

- **node.hpp**: Base class for all DOM nodes
- **element.hpp**: Element nodes with attributes
- **document.hpp**: Document root and factory methods
- **text.hpp**: Text and comment nodes

### HTML (`src/html/`)

HTML5 parser following WHATWG specification.

```
Input Stream → Tokenizer → Token Stream → Tree Builder → DOM Tree
```

- **tokenizer.hpp**: State machine tokenizer (82 states)
- **parser.hpp**: High-level parsing interface
- **tree_builder.hpp**: Constructs DOM from tokens

### CSS (`src/css/`)

CSS parser and style resolution.

```
CSS Text → Tokenizer → Parser → Stylesheet
                                    ↓
            DOM + Stylesheet → Style Resolver → Computed Styles
```

- **tokenizer.hpp**: CSS tokenizer
- **parser.hpp**: CSS grammar parser
- **selector.hpp**: Selector parsing and matching
- **value.hpp**: CSS value types (Length, Color, etc.)
- **style_resolver.hpp**: Cascade algorithm and inheritance

### JavaScript (`src/js/`)

JavaScript engine with partial ES6 support.

```
Source Code → Lexer → Parser → AST → Compiler → Bytecode → VM
```

Components:
- **lexer.hpp**: Token generation
- **parser.hpp**: Recursive descent parser, AST generation
- **ast.hpp**: Abstract Syntax Tree nodes
- **compiler.hpp**: Bytecode compilation
- **bytecode.hpp**: Instruction set definition
- **vm.hpp**: Stack-based virtual machine
- **value.hpp**: JavaScript value representation
- **object.hpp**: Object, Array, Function, Class
- **gc.hpp**: Mark-and-sweep garbage collector

### Text (`src/text/`)

Text processing and rendering.

- **font.hpp**: Font loading and metrics
- **shaper.hpp**: Text shaping (glyph positioning)
- **line_breaker.hpp**: Line breaking algorithm (UAX #14)

### Layout (`src/layout/`)

CSS layout engine.

- **box.hpp**: Box model representation
- **layout_context.hpp**: Layout state and engine
- **block_layout.hpp**: Block formatting context
- **inline_layout.hpp**: Inline formatting context

### Render (`src/render/`)

Rendering pipeline.

- **display_list.hpp**: Display commands
- **paint_context.hpp**: Command execution
- **compositor.hpp**: Layer compositing

### Bindings (`src/bindings/`)

JavaScript-DOM bindings.

- **dom_bindings.hpp**: DOM API exposure to JS

### Network (`src/network/`)

Network layer.

- **http_client.hpp**: HTTP/HTTPS requests
- **resource_loader.hpp**: Resource fetching with caching

### Browser (`src/browser/`)

Main browser application.

- **engine.hpp**: Coordinates all components
- **main.cpp**: Entry point

## Data Flow

### Page Load Sequence

1. **URL Request**: Network module fetches the document
2. **HTML Parsing**: Tokenizer + Tree Builder create DOM
3. **Resource Loading**: CSS, JS, images loaded in parallel
4. **Style Resolution**: CSS rules matched to elements
5. **Layout**: Box tree computed, positions calculated
6. **Paint**: Display list generated
7. **Composite**: Layers combined and presented

### JavaScript Execution

1. Script tag encountered or event fired
2. Source code lexed and parsed to AST
3. AST compiled to bytecode
4. VM executes bytecode
5. DOM modifications trigger re-layout/repaint

## Memory Management

- **RefPtr**: Reference-counted smart pointers for DOM nodes
- **Arena**: Fast allocation for temporary parsing structures
- **Pool**: Fixed-size object pools
- **GC**: Mark-and-sweep for JavaScript objects

## Threading Model

Current design is single-threaded with plans for:
- Separate thread for network I/O
- Worker threads for JavaScript
- Parallel style resolution

## Testing Strategy

### Unit Tests (`tests/unit/`)

Each module has dedicated unit tests:
- Core types and string operations
- HTML tokenizer state machine
- CSS selector matching
- JavaScript lexer/parser

### Integration Tests (`tests/integration/`)

End-to-end testing:
- HTML → DOM → Style → Layout pipeline
- JavaScript + DOM interaction

### Conformance Tests

- html5lib-tests for HTML parsing
- CSS Test Suites (subset)
- Test262 for JavaScript (subset)

## Build System

- CMake 3.20+ for configuration
- Conan 2.x for dependency management
- C++20 standard

## External Dependencies

| Library | Purpose | Optional |
|---------|---------|----------|
| FreeType | Font loading | Yes |
| libcurl | HTTP client | Yes |
| Google Test | Unit testing | Test only |

## Future Improvements

1. **Performance**: Parallel layout, GPU compositing
2. **Features**: Flexbox, Grid, more ES6
3. **Standards**: Better spec compliance
4. **Security**: Process isolation, sandboxing
