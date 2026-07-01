# Hawk — Project Design Philosophy

## Purpose

Hawk is a CLI-first, offline log analysis tool written in C++ for forensic and incident-response workflows.

Its purpose is to help analysts explore large log files without relying on heavy platforms, ad-hoc scripts, or tools that obscure evidence handling. Hawk is designed for correctness, reproducibility, and clear intent.

## Who Hawk Is For

Hawk is built for:

- digital forensics analysts,
- incident responders,
- security engineers,
- and technically inclined analysts.

These users are not necessarily programmers, and common investigation tasks should not require Python, SQL, or custom scripts.

Hawk is not intended for:

- casual log viewing,
- real-time monitoring,
- large-scale ingestion pipelines,
- data science workflows,
- or general-purpose database use.

## Core Belief

Logs are evidence, not just text.

Hawk treats logs as immutable input data whose integrity and provenance matter. Operations should be explainable, reproducible, and traceable back to the original file. Hawk avoids silently rewriting or normalizing source data in ways that could obscure evidence.

## Working Style

Investigations are iterative, not one-shot.

Users should be able to refine hypotheses, change filters, and build understanding gradually without mutating the underlying data. Hawk supports this by maintaining session state and by treating analysis as a sequence of explicit, replayable actions.

## Intent Over Output

Hawk is designed to capture what the user did, not just what they saw.

Replaying an analysis should be as simple as re-running the same commands on the same input. When persistence exists, it should favor recorded intent over opaque derived state.

## What Hawk Is

Hawk is:

- a CLI-first interactive log analysis tool,
- optimized for large files,
- designed around session-based exploration,
- built as a library-first system with a thin CLI,
- explicit about assumptions such as delimiter, headers, and schema,
- schema-aware with type-strict filtering and datetime pattern recognition,
- and focused on deterministic, explainable behaviour.

## What Hawk Is Not

Hawk is explicitly not:

- a SIEM,
- a log ingestion or shipping system,
- a replacement for Splunk, ELK, or similar platforms,
- a GUI-first application,
- a real-time monitoring system,
- a data science or notebook environment,
- or a general-purpose database engine.

These are deliberate non-goals.

## Architecture Principles

### 1. CLI First, Always

The command line is the primary interface. All core functionality must be accessible without a GUI.

Any future UI must be a client of the same core engine, not a separate special case.

### 2. Library First Architecture

Hawk is built around a reusable core library, `libhawk`.

- Core domain logic lives in the library.
- The CLI handles interaction and rendering.
- The core library must not contain console I/O.
- The engine must not make UI-specific assumptions.

This separation improves testability, scripting, automation, and future frontend support.

### 3. Clear Responsibility Boundaries

Hawk separates physical file access from logical record handling and parsing.

- `LogFile` provides memory-mapped raw file access.
- `SessionBuilder‍` owns file access and pre-session construction.
- `SessionConfig` is always complete before Session is constructed.
- `TypeInferrer` infers column types from raw records, produces Schema.
- `RecordSource` provides logical record access.
- `RecordParser` parses records into fields.
- `Session` manages state and schema.
- `Schema` holds `vector<ColumnSchema>` — names, types, nullable flags.
- `View` represents derived subsets of records as index maps over the source.
- `Projection` tracks the active column subset.
- Commands express user intent.
- Renderers present results.

No layer should leak responsibilities into another.

### 4. Commands Are Declarative

Commands describe what to do, not how to do it.

They operate on session state, return structured results, and avoid side effects where possible. This supports scripting, replayability, and automation.

### 5. Structured Results Over Text

The core engine returns typed results, not formatted output.

Formatting is a presentation concern handled by the CLI or other frontends. This supports multiple renderers, better testability, and long-term extensibility.

### 6. Reproducibility Over Convenience

Hawk prefers explicit user decisions, visible assumptions, and repeatable workflows over hidden heuristics, silent corrections, or "magic" behaviour.

Inference may exist, but it must be transparent, overrideable, and predictable.

### 7. Schema-Aware, Not Schema-Heavy

Logs may be treated as structured data, but Hawk should not become a database.

Schema handling should remain lightweight, derived from the source, and compatible with both positional and named fields. Structure exists to aid reasoning, not to obscure the raw data.

### 8. Views Over Mutation

Derived data should be represented as views, not rewritten logs.

Filtering, projection, and transformations should preserve the original data, remain reversible when possible, and make lineage understandable.

### 9. Performance Matters, but Not at the Cost of Clarity

Hawk must handle large files efficiently through streaming where possible, selective indexing, and careful memory use.

Performance optimizations must not obscure correctness, complicate the mental model, or make behaviour harder to reason about.

### 10. Predictable Behaviour Beats Cleverness

Hawk should avoid being too smart.

It should favour obvious behaviour, stable mental models, and consistency across commands. Surprising tools are dangerous in investigative workflows.

### 11. Persistence Is Optional

Sessions are ephemeral by default.

Where persistence exists — currently the `history --save` command — it favours recorded intent (the commands themselves) over opaque derived state.

## Current Design Direction

Hawk is an offline, interactive investigation tool for large logs.

The core design favors:

- deterministic behaviour,
- memory-efficient access,
- clear separation of concerns,
- explicit interpretation,
- and a minimal, composable command surface.

Type inference is transparent and overrideable. Datetime columns carry an explicit pattern string derived from inference or set by the analyst, used for type-correct comparisons.

The project should continue to feel like an investigation tool, not a database or a platform.

## Success Criteria

Hawk is successful if:

- an analyst can explore a large log file without writing code,
- an analysis can be reproduced by rerunning a script,
- the tool behaves predictably and explainably,
- the architecture remains understandable as features grow,
- and the codebase reflects deliberate, thoughtful system design.

Mass adoption, enterprise deployment, or a large feature set are not required for success.

## Final Note

Hawk is a **serious engineering project**, not a demo or a toy.

It values restraint, correctness, and clarity over breadth or trend-chasing. New features should be added only when they preserve the mental model, respect the architecture, and strengthen the core philosophy.

If a feature makes Hawk harder to explain, harder to replay, or harder to keep deterministic, it is likely the wrong feature.
