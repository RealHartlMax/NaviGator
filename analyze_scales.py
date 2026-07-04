#!/usr/bin/env python3
"""Deep dive into handle calculation to find exact algorithm"""

import math
from pathlib import Path

def parse_curve_point(line):
    parts = line.strip().split()
    if not parts or parts[0] != 'c':
        return None
    try:
        return {
            'pos': (float(parts[1]), float(parts[2]), float(parts[3])),
            'handleA': (float(parts[4]), float(parts[5]), float(parts[6])),
            'handleB': (float(parts[7]), float(parts[8]), float(parts[9])),
        }
    except (ValueError, IndexError):
        return None

def vec_distance(p1, p2):
    return math.sqrt((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2 + (p1[2]-p2[2])**2)

def vec_subtract(p1, p2):
    return (p1[0]-p2[0], p1[1]-p2[1], p1[2]-p2[2])

def vec_normalize(v):
    length = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    if length < 0.0001:
        return (1.0, 0.0, 0.0)
    return (v[0]/length, v[1]/length, v[2]/length)

def vec_scale(v, s):
    return (v[0]*s, v[1]*s, v[2]*s)

def vec_add(v1, v2):
    return (v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2])

def vec_magnitude(v):
    return math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)

# Load data
track_path = Path("G:/dist-debug/Neuer Ordner/trains1.dat")
with open(track_path, 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

header = lines[0].strip().split()
loop_type = header[2]

points = []
for line in lines[1:]:
    line = line.strip()
    if not line:
        continue
    if line.startswith('c '):
        point = parse_curve_point(line)
        if point:
            points.append(point)

print("=" * 90)
print("DEEP ANALYSIS: Extracting Handle Scale Factors from Actual Data")
print("=" * 90)

is_closed = loop_type == 'close'
n = len(points)

# For each point, try to reverse-engineer what scale factor was used
scales_A = []
scales_B = []
n_tests = 0

for i in range(min(50, n)):
    point = points[i]
    pos = point['pos']
    handleA = point['handleA']
    handleB = point['handleB']
    
    prev_idx = (i - 1) % n if is_closed else (i - 1 if i > 0 else None)
    next_idx = (i + 1) % n if is_closed else (i + 1 if i < n - 1 else None)
    
    prev_pos = points[prev_idx]['pos'] if prev_idx is not None else None
    next_pos = points[next_idx]['pos'] if next_idx is not None else None
    
    if not (prev_pos and next_pos):
        continue
    
    # Calculate tangent direction
    tangent = vec_subtract(next_pos, prev_pos)
    handle_dir = vec_normalize(tangent)
    
    prev_dist = vec_distance(pos, prev_pos)
    next_dist = vec_distance(pos, next_pos)
    
    # Reverse-engineer the scale factor used
    # handleA = pos - dir * (prev_dist * scale)
    # So: dir * (prev_dist * scale) = pos - handleA
    
    if prev_dist > 0.01:
        offset_A = vec_subtract(pos, handleA)  # This should be dir * (prev_dist * scale)
        magnitude_A = vec_magnitude(offset_A)
        implied_scale_A = magnitude_A / prev_dist
        scales_A.append(implied_scale_A)
    
    if next_dist > 0.01:
        offset_B = vec_subtract(handleB, pos)  # This should be dir * (next_dist * scale)
        magnitude_B = vec_magnitude(offset_B)
        implied_scale_B = magnitude_B / next_dist
        scales_B.append(implied_scale_B)
    
    n_tests += 1

import statistics

if scales_A and scales_B:
    print(f"\nAnalyzed {n_tests} curve points with both neighbors\n")
    
    print("Handle A (previous side) scale factors:")
    print(f"  Mean:     {statistics.mean(scales_A):.6f}")
    print(f"  Median:   {statistics.median(scales_A):.6f}")
    print(f"  Std Dev:  {statistics.stdev(scales_A):.6f}")
    print(f"  Min/Max:  {min(scales_A):.6f} / {max(scales_A):.6f}")
    
    print("\nHandle B (next side) scale factors:")
    print(f"  Mean:     {statistics.mean(scales_B):.6f}")
    print(f"  Median:   {statistics.median(scales_B):.6f}")
    print(f"  Std Dev:  {statistics.stdev(scales_B):.6f}")
    print(f"  Min/Max:  {min(scales_B):.6f} / {max(scales_B):.6f}")
    
    # Check if scale is consistent
    all_scales = scales_A + scales_B
    overall_mean = statistics.mean(all_scales)
    overall_std = statistics.stdev(all_scales)
    
    print(f"\nOverall:")
    print(f"  Mean scale factor: {overall_mean:.6f}")
    print(f"  Standard deviation: {overall_std:.6f}")
    
    print("\n" + "=" * 90)
    print("INTERPRETATION")
    print("=" * 90)
    
    if overall_std < 0.01:
        print(f"✓ Very consistent scale factor: {overall_mean:.6f}")
        print(f"  This suggests handles use fixed scale: ~1/{1/overall_mean:.2f}")
    elif overall_std < 0.05:
        print(f"⚠️  Mostly consistent scale: {overall_mean:.6f} ± {overall_std:.6f}")
        print(f"  Minor variations in scale, possibly per-segment")
    else:
        print(f"🔴 Highly variable scale factors")
        print(f"  Mean: {overall_mean:.6f}, StdDev: {overall_std:.6f}")
        print(f"  This suggests a completely different algorithm")
    
    # List common fractions
    print("\nClosest standard fractions:")
    for numerator in range(1, 11):
        for denominator in range(2, 11):
            frac = numerator / denominator
            error = abs(frac - overall_mean)
            if error < 0.01:
                print(f"  {numerator}/{denominator} = {frac:.6f} (error: {error:.6f})")

print("\n" + "=" * 90)
print("HYPOTHESIS: Chord-based scaling?")
print("=" * 90)
print("""
Some curve algorithms use:
  - handleA = pos - direction * (chord_length * scale)
  - Where chord_length = distance(prev, next)

Instead of individual distances. Let's test this...
""")

# Test chord-length hypothesis
chord_scales = []
for i in range(min(50, n)):
    point = points[i]
    pos = point['pos']
    handleA = point['handleA']
    handleB = point['handleB']
    
    prev_idx = (i - 1) % n if is_closed else (i - 1 if i > 0 else None)
    next_idx = (i + 1) % n if is_closed else (i + 1 if i < n - 1 else None)
    
    prev_pos = points[prev_idx]['pos'] if prev_idx is not None else None
    next_pos = points[next_idx]['pos'] if next_idx is not None else None
    
    if not (prev_pos and next_pos):
        continue
    
    tangent = vec_subtract(next_pos, prev_pos)
    chord_length = vec_magnitude(tangent)
    
    if chord_length > 0.01:
        handle_dir = vec_normalize(tangent)
        offset_A = vec_subtract(pos, handleA)
        offset_B = vec_subtract(handleB, pos)
        
        scale_A = vec_magnitude(offset_A) / chord_length
        scale_B = vec_magnitude(offset_B) / chord_length
        
        chord_scales.extend([scale_A, scale_B])

if chord_scales:
    print(f"Chord-length based scale factors:")
    print(f"  Mean: {statistics.mean(chord_scales):.6f}")
    print(f"  StdDev: {statistics.stdev(chord_scales):.6f}")
    print(f"  Range: {min(chord_scales):.6f} - {max(chord_scales):.6f}")
