#!/usr/bin/env python3
"""Analyze track files for curve calculation issues"""

import os
from pathlib import Path

track_dir = Path("G:/dist-debug/Neuer Ordner")
tracks_to_analyze = [
    "trains1.dat",
    "mexico_main.dat", 
    "pc_train_track_bw.dat",
    "sd_extended_01.dat",
    "pc_train_track_val.dat",
    "trains_old_west01.dat"
]

def analyze_track(filepath):
    """Analyze a single track file"""
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        return None
    
    if not lines:
        return None
    
    # Parse header
    header_parts = lines[0].strip().split()
    total_nodes = int(header_parts[0]) if len(header_parts) > 0 else 0
    curve_nodes_claimed = int(header_parts[1]) if len(header_parts) > 1 else 0
    track_type = header_parts[2] if len(header_parts) > 2 else "unknown"
    
    # Count actual curves in file
    curve_count = 0
    linear_count = 0
    handle_errors = 0
    
    for i, line in enumerate(lines[1:], 1):
        line = line.strip()
        if not line:
            continue
        
        if line.startswith('c '):
            curve_count += 1
            # Verify curve point has correct format
            parts = line.split()
            if len(parts) < 11:  # c + 9 coords + scalar + info
                handle_errors += 1
        else:
            # Should be linear point
            parts = line.split()
            if len(parts) > 0 and not parts[0].startswith('c'):
                linear_count += 1
    
    total_data_lines = curve_count + linear_count
    
    return {
        'filename': filepath.name,
        'total_nodes_claimed': total_nodes,
        'curve_nodes_claimed': curve_nodes_claimed,
        'track_type': track_type,
        'actual_curves': curve_count,
        'actual_linear': linear_count,
        'total_data_lines': total_data_lines,
        'handle_errors': handle_errors,
        'mismatch': curve_count != curve_nodes_claimed,
        'file_lines': len(lines)
    }

print("=" * 90)
print("TRACK FILE ANALYSIS - CURVE CALCULATION VERIFICATION")
print("=" * 90)

results = []
for track_name in tracks_to_analyze:
    track_path = track_dir / track_name
    if track_path.exists():
        result = analyze_track(track_path)
        if result:
            results.append(result)

# Print results
for r in results:
    print(f"\n{r['filename']}")
    print("-" * 80)
    print(f"  Header claims: {r['total_nodes_claimed']} total, {r['curve_nodes_claimed']} curves ({r['track_type']})")
    print(f"  Actual data:   {r['actual_curves']} curves, {r['actual_linear']} linear (total: {r['total_data_lines']})")
    
    if r['curve_nodes_claimed'] == 0:
        print(f"  ⚠️  ZERO CURVE NODES IN HEADER - All {r['actual_curves']} points marked as curves")
    elif r['mismatch']:
        percent_expected = (r['curve_nodes_claimed'] / r['total_nodes_claimed'] * 100) if r['total_nodes_claimed'] > 0 else 0
        percent_actual = (r['actual_curves'] / r['total_data_lines'] * 100) if r['total_data_lines'] > 0 else 0
        print(f"  ⚠️  CURVE MISMATCH: Expected {r['curve_nodes_claimed']} ({percent_expected:.1f}%), found {r['actual_curves']} ({percent_actual:.1f}%)")
    else:
        print(f"  ✓  Match: {r['curve_nodes_claimed']} claimed = {r['actual_curves']} actual")
    
    if r['handle_errors'] > 0:
        print(f"  ⚠️  ERROR: {r['handle_errors']} curve points with incomplete handle data")

print("\n" + "=" * 90)
print("SUMMARY")
print("=" * 90)

# Check for specific patterns
zero_curve_tracks = [r for r in results if r['curve_nodes_claimed'] == 0]
if zero_curve_tracks:
    print(f"\n🔴 ISSUE FOUND: {len(zero_curve_tracks)} track(s) with zero curve nodes in header:")
    for r in zero_curve_tracks:
        print(f"   - {r['filename']}: Claims {r['curve_nodes_claimed']} curves but file has {r['actual_curves']}")

mismatch_tracks = [r for r in results if r['mismatch'] and r['curve_nodes_claimed'] > 0]
if mismatch_tracks:
    print(f"\n🟡 WARNING: {len(mismatch_tracks)} track(s) with curve count mismatch:")
    for r in mismatch_tracks:
        print(f"   - {r['filename']}: Claimed {r['curve_nodes_claimed']}, actual {r['actual_curves']}")

error_tracks = [r for r in results if r['handle_errors'] > 0]
if error_tracks:
    print(f"\n🔴 ERROR: {len(error_tracks)} track(s) with malformed curve data:")
    for r in error_tracks:
        print(f"   - {r['filename']}: {r['handle_errors']} points with missing handles")

if not zero_curve_tracks and not mismatch_tracks and not error_tracks:
    print("\n✓ No issues detected in curve data format")
