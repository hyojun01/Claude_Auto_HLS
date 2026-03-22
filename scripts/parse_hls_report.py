#!/usr/bin/env python3
"""Vitis HLS csynth.xml report parser.

Extracts synthesis metrics into structured JSON for agent consumption.

Usage:
    python3 parse_hls_report.py parse <csynth.xml>
    python3 parse_hls_report.py compare <baseline.xml> <optimized.xml>
    python3 parse_hls_report.py check <csynth.xml> [--target-ii N] [--max-util PCT]
"""

import argparse
import json
import os
import re
import sys
import xml.etree.ElementTree as ET


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _safe_text(element, tag):
    """Return stripped text of a child element, or None if missing/undef."""
    child = element.find(tag)
    if child is None or child.text is None:
        return None
    text = child.text.strip()
    if text in ("undef", "-", ""):
        return None
    return text


def _safe_float(element, tag):
    text = _safe_text(element, tag)
    if text is None:
        return None
    try:
        return float(text)
    except ValueError:
        return None


def _safe_int(element, tag):
    text = _safe_text(element, tag)
    if text is None:
        return None
    try:
        return int(text)
    except ValueError:
        return None


def _util_pct(used, available):
    if available is None or available == 0:
        return 0.0
    return round(used / available * 100, 1)


# ---------------------------------------------------------------------------
# Section parsers
# ---------------------------------------------------------------------------

def _parse_design(root):
    ua = root.find("UserAssignments")
    if ua is None:
        return {}
    return {
        "part": _safe_text(ua, "Part"),
        "product_family": _safe_text(ua, "ProductFamily"),
        "top_function": _safe_text(ua, "TopModelName"),
        "target_clock_ns": _safe_float(ua, "TargetClockPeriod"),
        "clock_uncertainty_ns": _safe_float(ua, "ClockUncertainty"),
        "flow_target": _safe_text(ua, "FlowTarget"),
    }


def _parse_timing(root):
    design = _parse_design(root)
    target = design.get("target_clock_ns")

    pe = root.find("PerformanceEstimates")
    if pe is None:
        return {"target_ns": target, "estimated_ns": None, "slack_ns": None, "met": None}

    sta = pe.find("SummaryOfTimingAnalysis")
    estimated = _safe_float(sta, "EstimatedClockPeriod") if sta is not None else None

    slack = None
    met = None
    if target is not None and estimated is not None:
        slack = round(target - estimated, 3)
        met = estimated <= target

    return {
        "target_ns": target,
        "estimated_ns": estimated,
        "slack_ns": slack,
        "met": met,
    }


def _normalize_pipeline_type(raw):
    if raw is None:
        return "none"
    low = raw.lower()
    if "dataflow" in low:
        return "dataflow"
    if "loop" in low or "pipeline" in low:
        return "loop"
    return "none"


def _parse_latency(root):
    pe = root.find("PerformanceEstimates")
    if pe is None:
        return {}

    pt_el = pe.find("PipelineType")
    pipeline_type = _normalize_pipeline_type(pt_el.text if pt_el is not None else None)

    sol = pe.find("SummaryOfOverallLatency")
    if sol is None:
        return {"pipeline_type": pipeline_type}

    return {
        "pipeline_type": pipeline_type,
        "best_cycles": _safe_int(sol, "Best-caseLatency"),
        "average_cycles": _safe_int(sol, "Average-caseLatency"),
        "worst_cycles": _safe_int(sol, "Worst-caseLatency"),
        "best_time": _safe_text(sol, "Best-caseRealTimeLatency"),
        "average_time": _safe_text(sol, "Average-caseRealTimeLatency"),
        "worst_time": _safe_text(sol, "Worst-caseRealTimeLatency"),
        "interval_min": _safe_int(sol, "Interval-min"),
        "interval_max": _safe_int(sol, "Interval-max"),
        "dataflow_throughput_cycles": _safe_int(sol, "DataflowPipelineThroughput"),
    }


def _parse_loop_latency(root):
    pe = root.find("PerformanceEstimates")
    if pe is None:
        return []

    sll = pe.find("SummaryOfLoopLatency")
    if sll is None:
        return []

    loops = []
    for loop_el in sll:
        loops.append({
            "name": loop_el.tag,
            "pipeline_ii": _safe_int(loop_el, "PipelineII"),
            "pipeline_depth": _safe_int(loop_el, "PipelineDepth"),
            "trip_count": _safe_int(loop_el, "TripCount"),
            "slack": _safe_float(loop_el, "Slack"),
            "latency_cycles": _safe_int(loop_el, "Latency"),
        })
    return loops


_RESOURCE_KEYS = ["BRAM_18K", "DSP", "FF", "LUT", "URAM"]
_RESOURCE_NAMES = ["bram", "dsp", "ff", "lut", "uram"]


def _parse_resources(root):
    ae = root.find("AreaEstimates")
    if ae is None:
        return {}

    used_el = ae.find("Resources")
    avail_el = ae.find("AvailableResources")

    result = {}
    for xml_key, name in zip(_RESOURCE_KEYS, _RESOURCE_NAMES):
        used = _safe_int(used_el, xml_key) if used_el is not None else None
        avail = _safe_int(avail_el, xml_key) if avail_el is not None else None
        used = used if used is not None else 0
        avail = avail if avail is not None else 0
        result[name] = {
            "used": used,
            "available": avail,
            "util_pct": _util_pct(used, avail),
        }
    return result


def _parse_interfaces(root):
    intf = root.find("InterfaceSummary")
    if intf is None:
        return []

    # Group RTL ports by logical interface name derived from the port name.
    # s_axi ports:  s_axi_<bundle>_<SIGNAL> -> group by <bundle>
    # axis ports:   <port>_T<SIGNAL>         -> group by <port>
    # ap_ctrl_hs:   ap_clk, ap_rst_n, interrupt -> skip (infrastructure)
    groups = {}  # key: (logical_name, protocol) -> list of port dicts

    for port_el in intf.findall("RtlPorts"):
        name = _safe_text(port_el, "name") or ""
        protocol = _safe_text(port_el, "IOProtocol") or ""
        direction = _safe_text(port_el, "Dir") or ""
        bits_val = _safe_int(port_el, "Bits") or 0
        attribute = _safe_text(port_el, "Attribute") or ""

        # Skip infrastructure signals
        if protocol == "ap_ctrl_hs":
            continue

        # Determine logical group name and signal suffix
        if protocol == "s_axi":
            # s_axi_control_AWVALID -> group "control", signal "AWVALID"
            m = re.match(r"s_axi_(\w+?)_([A-Z]+)$", name)
            if m:
                group_name = m.group(1)
                signal = m.group(2)
            else:
                group_name = name
                signal = name
        elif protocol == "axis":
            # in_r_TDATA -> group "in_r", signal "TDATA"
            m = re.match(r"(.+?)_(T[A-Z]+)$", name)
            if m:
                group_name = m.group(1)
                signal = m.group(2)
            else:
                group_name = name
                signal = name
        else:
            # Other protocols: group by Object or name prefix
            obj = _safe_text(port_el, "Object") or name
            group_name = obj
            signal = name

        key = (group_name, protocol)
        if key not in groups:
            groups[key] = {"dirs": set(), "signals": [], "max_data_bits": 0}

        groups[key]["dirs"].add(direction)
        groups[key]["signals"].append(signal)
        is_data = (attribute == "data"
                   or signal in ("TDATA", "WDATA", "RDATA"))
        if is_data and bits_val > groups[key]["max_data_bits"]:
            groups[key]["max_data_bits"] = bits_val

    interfaces = []
    for (group_name, protocol), info in groups.items():
        dirs = info["dirs"]
        if dirs == {"in"}:
            direction = "in"
        elif dirs == {"out"}:
            direction = "out"
        else:
            direction = "both"

        interfaces.append({
            "name": group_name,
            "protocol": protocol,
            "direction": direction,
            "data_width": info["max_data_bits"],
            "signals": sorted(set(info["signals"])),
        })

    # Sort: data interfaces first (axis), then control (s_axi)
    interfaces.sort(key=lambda x: (x["protocol"] != "axis", x["name"]))
    return interfaces


def _parse_violations(root):
    pe = root.find("PerformanceEstimates")
    if pe is None:
        return {"timing_violation": False, "details": []}

    sv = pe.find("SummaryOfViolations")
    if sv is None:
        return {"timing_violation": False, "details": []}

    viol_type = _safe_text(sv, "ViolationType")
    has_violation = viol_type is not None

    details = []
    loop_viol = sv.find("SummaryOfLoopViolations")
    if loop_viol is not None:
        for lv in loop_viol:
            li = _safe_text(lv, "ViolationType")
            if li is not None:
                details.append({
                    "loop": _safe_text(lv, "Name") or lv.tag,
                    "issue": li,
                    "type": _safe_text(lv, "ViolationType"),
                    "source": _safe_text(lv, "SourceLocation"),
                })

    return {"timing_violation": has_violation, "details": details}


# ---------------------------------------------------------------------------
# Top-level commands
# ---------------------------------------------------------------------------

def parse_report(xml_path):
    """Parse a csynth.xml and return a metrics dict."""
    tree = ET.parse(xml_path)
    root = tree.getroot()

    version_el = root.find("ReportVersion/Version")
    version = version_el.text.strip() if version_el is not None and version_el.text else None

    return {
        "report_version": version,
        "design": _parse_design(root),
        "timing": _parse_timing(root),
        "latency": _parse_latency(root),
        "loops": _parse_loop_latency(root),
        "resources": _parse_resources(root),
        "interfaces": _parse_interfaces(root),
        "violations": _parse_violations(root),
        "source_file": os.path.abspath(xml_path),
    }


def compare_reports(baseline, optimized):
    """Compare two parsed report dicts and return a delta summary."""

    def _delta(a, b):
        if a is None or b is None:
            return None
        return round(b - a, 3)

    def _delta_pct(a, b):
        if a is None or b is None or a == 0:
            return None
        return round((b - a) / a * 100, 1)

    def _speedup(a, b):
        if a is None or b is None or b == 0:
            return None
        return round(a / b, 2)

    bt = baseline["timing"]
    ot = optimized["timing"]
    bl = baseline["latency"]
    ol = optimized["latency"]

    result = {
        "baseline": {
            "source_file": baseline.get("source_file"),
            "top_function": baseline["design"].get("top_function"),
            "part": baseline["design"].get("part"),
        },
        "optimized": {
            "source_file": optimized.get("source_file"),
            "top_function": optimized["design"].get("top_function"),
            "part": optimized["design"].get("part"),
        },
        "timing": {
            "baseline_ns": bt.get("estimated_ns"),
            "optimized_ns": ot.get("estimated_ns"),
            "delta_ns": _delta(bt.get("estimated_ns"), ot.get("estimated_ns")),
            "both_met": (bt.get("met") is True) and (ot.get("met") is True),
        },
        "latency": {
            "baseline_worst_cycles": bl.get("worst_cycles"),
            "optimized_worst_cycles": ol.get("worst_cycles"),
            "delta_cycles": _delta(bl.get("worst_cycles"), ol.get("worst_cycles")),
            "speedup": _speedup(bl.get("worst_cycles"), ol.get("worst_cycles")),
            "baseline_interval": bl.get("interval_min"),
            "optimized_interval": ol.get("interval_min"),
            "delta_interval": _delta(bl.get("interval_min"), ol.get("interval_min")),
        },
        "resources": {},
        "loops_comparison": {
            "baseline_loops": [l["name"] for l in baseline.get("loops", [])],
            "optimized_loops": [l["name"] for l in optimized.get("loops", [])],
            "pipeline_type_changed": bl.get("pipeline_type") != ol.get("pipeline_type"),
            "baseline_type": bl.get("pipeline_type"),
            "optimized_type": ol.get("pipeline_type"),
        },
    }

    br = baseline["resources"]
    or_ = optimized["resources"]
    for name in _RESOURCE_NAMES:
        bu = br.get(name, {}).get("used")
        ou = or_.get(name, {}).get("used")
        result["resources"][name] = {
            "baseline": bu,
            "optimized": ou,
            "delta": _delta(bu, ou),
            "delta_pct": _delta_pct(bu, ou),
        }

    return result


def check_anomalies(report, target_ii=None, max_util=80):
    """Check a parsed report for anomalies. Returns a dict with anomaly list."""
    anomalies = []

    # 1. Timing violation
    t = report["timing"]
    if t.get("met") is False:
        anomalies.append({
            "severity": "error",
            "category": "timing_violation",
            "message": (
                f"Timing not met: estimated {t['estimated_ns']} ns > "
                f"target {t['target_ns']} ns (slack: {t['slack_ns']} ns)"
            ),
        })

    # 2. Resource overutilization
    for name in _RESOURCE_NAMES:
        r = report["resources"].get(name, {})
        pct = r.get("util_pct", 0)
        if pct > max_util:
            anomalies.append({
                "severity": "warning",
                "category": "high_utilization",
                "resource": name,
                "message": (
                    f"{name.upper()} utilization at {pct}% "
                    f"({r['used']:,} / {r['available']:,}) exceeds {max_util}% threshold"
                ),
            })

    # 3. Missed II
    if target_ii is not None:
        for loop in report.get("loops", []):
            ii = loop.get("pipeline_ii")
            if ii is not None and ii > target_ii:
                anomalies.append({
                    "severity": "warning",
                    "category": "ii_missed",
                    "loop": loop["name"],
                    "message": (
                        f"{loop['name']} achieved II={ii}, target was II={target_ii}"
                    ),
                })

    # 4. Undefined latency (informational)
    lat = report.get("latency", {})
    if lat.get("worst_cycles") is None:
        anomalies.append({
            "severity": "info",
            "category": "undef_latency",
            "message": "Worst-case latency is undefined (streaming design with variable input length)",
        })

    # 5. XML-reported violations
    viol = report.get("violations", {})
    if viol.get("timing_violation"):
        # Only add if not already caught by numeric check
        if t.get("met") is not False:
            anomalies.append({
                "severity": "error",
                "category": "timing_violation",
                "message": "Timing violation reported in synthesis XML",
            })

    errors = sum(1 for a in anomalies if a["severity"] == "error")
    warnings = sum(1 for a in anomalies if a["severity"] == "warning")
    infos = sum(1 for a in anomalies if a["severity"] == "info")

    return {
        "source_file": report.get("source_file"),
        "anomalies": anomalies,
        "summary": {
            "errors": errors,
            "warnings": warnings,
            "infos": infos,
            "passed": errors == 0,
        },
    }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Parse Vitis HLS csynth.xml reports into structured JSON."
    )
    sub = parser.add_subparsers(dest="command")

    # parse
    p_parse = sub.add_parser("parse", help="Extract metrics from a csynth.xml")
    p_parse.add_argument("xml", help="Path to csynth.xml")

    # compare
    p_cmp = sub.add_parser("compare", help="Compare two csynth.xml reports")
    p_cmp.add_argument("baseline", help="Baseline csynth.xml")
    p_cmp.add_argument("optimized", help="Optimized csynth.xml")

    # check
    p_chk = sub.add_parser("check", help="Check report for anomalies")
    p_chk.add_argument("xml", help="Path to csynth.xml")
    p_chk.add_argument("--target-ii", type=int, default=None,
                        help="Target initiation interval (flag loops exceeding this)")
    p_chk.add_argument("--max-util", type=float, default=80,
                        help="Max utilization %% threshold (default: 80)")

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        sys.exit(2)

    try:
        if args.command == "parse":
            if not os.path.isfile(args.xml):
                print(f"Error: file not found: {args.xml}", file=sys.stderr)
                sys.exit(2)
            result = parse_report(args.xml)

        elif args.command == "compare":
            for path in (args.baseline, args.optimized):
                if not os.path.isfile(path):
                    print(f"Error: file not found: {path}", file=sys.stderr)
                    sys.exit(2)
            b = parse_report(args.baseline)
            o = parse_report(args.optimized)
            result = compare_reports(b, o)

        elif args.command == "check":
            if not os.path.isfile(args.xml):
                print(f"Error: file not found: {args.xml}", file=sys.stderr)
                sys.exit(2)
            report = parse_report(args.xml)
            result = check_anomalies(report, args.target_ii, args.max_util)
            # Exit 1 if any errors
            json.dump(result, sys.stdout, indent=2)
            print()
            sys.exit(0 if result["summary"]["passed"] else 1)

        else:
            parser.print_help()
            sys.exit(2)

        json.dump(result, sys.stdout, indent=2)
        print()

    except ET.ParseError as e:
        print(f"Error: malformed XML: {e}", file=sys.stderr)
        sys.exit(2)


if __name__ == "__main__":
    main()
