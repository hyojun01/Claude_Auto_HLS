---
name: ip-documentation
description: IP documentation standards and templates for HLS IP reference documents — port tables, resource utilization, performance metrics, integration guides, and changelog conventions
user-invocable: false
---

# Skill: IP Documentation Standards

## Documentation Principles

1. **Accuracy**: Every number in the documentation must come from actual synthesis reports, not estimates
2. **Completeness**: Cover all aspects an integrator needs: what the IP does, how to connect it, and what performance to expect
3. **Clarity**: Use precise hardware terminology; avoid ambiguity
4. **Maintainability**: Use templates consistently so updates are straightforward

## Documentation File Structure

```
docs/<ip_name>/
├── README.md              # Complete IP reference document
├── integration_guide.md   # Step-by-step integration into Vivado
└── changelog.md           # Version history with metrics
```

## Writing Style Guide

### Tone
- Professional, technical, and concise
- Write for hardware engineers who will integrate the IP
- Assume the reader understands FPGA, AXI, and HLS concepts

### Structure
- Use headers for navigation (`##` for major sections, `###` for subsections)
- Use tables for structured data (ports, resources, performance)
- Use code blocks for commands, pragmas, and code snippets
- Use bullet points only for lists of distinct items

### Numbers and Units
- Always include units: `10 ns`, `256 cycles`, `100 MHz`
- Use consistent precision: don't mix `3.14` with `2.7182818`
- Resource counts are always integers
- Percentages to one decimal place: `45.2%`

### Port Documentation
Each port must specify:
- Name (exact C++ parameter name)
- Direction (IN, OUT, INOUT)
- Data type (C++ type and bit width)
- Interface protocol (AXI-Stream, AXI-Lite, etc.)
- Description (what the port carries, any constraints)

### Architecture Description
- Describe the design as a block diagram in text form
- Label all pipeline stages or DATAFLOW regions
- Describe data flow from input to output
- Note any internal state or static variables

### Performance Documentation
Always provide:
- Latency in both **cycles** and **nanoseconds** (at the configured clock)
- Initiation interval
- Maximum throughput (samples/sec or bytes/sec)
- Resource utilization as used/available/percentage

### Integration Documentation
- Exact Vivado version compatibility
- Export format (IP Catalog or .xo)
- Block diagram connection instructions
- Register map for AXI-Lite controlled IPs
- Timing constraints or special requirements

## README.md Sections Checklist

- [ ] Overview (1-2 paragraph summary)
- [ ] Specifications table (tool version, FPGA part, clock, top function)
- [ ] I/O Ports table (all ports with types and protocols)
- [ ] Functional Description (algorithm, data flow, FSM if any)
- [ ] Architecture (block diagram, pipeline stages)
- [ ] Performance (latency, II, throughput tables)
- [ ] Resource Utilization table
- [ ] Interface Details (control protocol, data format)
- [ ] Usage Example (minimal testbench snippet)
- [ ] Constraints & Limitations
- [ ] Optimization History (after optimization stage)

## Changelog Conventions

Follow Keep a Changelog format:
- `Added` — new features or files
- `Changed` — modifications to existing functionality
- `Fixed` — bug fixes
- `Optimized` — performance improvements (include before/after numbers)
- `Removed` — deleted features or deprecated items

Always include synthesis metrics with each version entry.
