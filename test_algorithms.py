#!/usr/bin/env python3
"""Analyze pattern of handle calculation differences"""

import math
from pathlib import Path
import statistics

def parse_curve_point(line):
    parts = line.strip().split()
    if not parts or parts[0] != 'c':
        return None
    try:
        return {
            'pos': (float(parts[1]), float(parts[2]), float(parts[3])),
            'handleA': (float(parts[4]), float(parts[5]), float(parts[6])),
            'handleB': (float(parts[7]), float(parts[8]), float(parts[9])),
            'scalar': float(parts[10]),
        }
    except (ValueError, IndexError):
        return None

def parse_linear_point(line):
    parts = line.strip().split()
    if not parts or parts[0] == 'c':
        return None
    try:
        return {'pos': (float(parts[0]), float(parts[1]), float(parts[2]))}
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

def test_algorithms(points, point_idx, loop_type):
    """Test different curve calculation algorithms"""
    SCALE = 1.0 / 3.0
    is_closed = loop_type == 'close'
    
    point = points[point_idx]
    if 'handleA' not in point:
        return None
    
    pos = point['pos']
    n = len(points)
    
    # Get neighbors
    prev_idx = (point_idx - 1) % n if is_closed else (point_idx - 1 if point_idx > 0 else None)
    next_idx = (point_idx + 1) % n if is_closed else (point_idx + 1 if point_idx < n - 1 else None)
    
    prev_pos = points[prev_idx]['pos'] if prev_idx is not None else None
    next_pos = points[next_idx]['pos'] if next_idx is not None else None
    
    actual_handleA = point['handleA']
    actual_handleB = point['handleB']
    
    results = {}
    
    # Algorithm 1: Catmull-Rom (prev->next tangent)
    if prev_pos and next_pos:
        tangent = vec_subtract(next_pos, prev_pos)
    elif next_pos:
        tangent = vec_subtract(next_pos, pos)
    elif prev_pos:
        tangent = vec_subtract(pos, prev_pos)
    else:
        tangent = (1.0, 0.0, 0.0)
    
    handle_dir = vec_normalize(tangent)
    prev_dist = vec_distance(pos, prev_pos) if prev_pos else 0.0
    next_dist = vec_distance(pos, next_pos) if next_pos else 0.0
    
    if prev_pos:
        expected_A_1 = vec_add(pos, vec_scale(handle_dir, -prev_dist * SCALE))
    else:
        expected_A_1 = pos
    
    if next_pos:
        expected_B_1 = vec_add(pos, vec_scale(handle_dir, next_dist * SCALE))
    else:
        expected_B_1 = pos
    
    error_A_1 = vec_distance(actual_handleA, expected_A_1)
    error_B_1 = vec_distance(actual_handleB, expected_B_1)
    
    results['catmull_rom'] = {
        'error_A': error_A_1,
        'error_B': error_B_1,
        'total': error_A_1 + error_B_1
    }
    
    # Algorithm 2: Simple linear interpolation scaled handles
    # Maybe they use a different scale factor?
    for test_scale in [0.25, 0.3, 0.33, 0.35, 0.4, 0.5]:
        if prev_pos:
            expected_A = vec_add(pos, vec_scale(handle_dir, -prev_dist * test_scale))
        else:
            expected_A = pos
        
        if next_pos:
            expected_B = vec_add(pos, vec_scale(handle_dir, next_dist * test_scale))
        else:
            expected_B = pos
        
        error = vec_distance(actual_handleA, expected_A) + vec_distance(actual_handleB, expected_B)
        results[f'scale_{test_scale}'] = {'error_A': vec_distance(actual_handleA, expected_A), 'error_B': vec_distance(actual_handleB, expected_B), 'total': error}
    
    return results

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
    else:
        point = parse_linear_point(line)
    if point:
        points.append(point)

print("=" * 90)
print("ALGORITHM TESTING - Finding best fit for handle calculations")
print("=" * 90)

# Test all algorithms on several points
best_results = {algo: 0 for algo in ['catmull_rom', 'scale_0.25', 'scale_0.3', 'scale_0.33', 'scale_0.35', 'scale_0.4', 'scale_0.5']}
total_results = {algo: [] for algo in best_results.keys()}

for idx in range(min(100, len(points))):  # Test first 100 points
    result = test_algorithms(points, idx, loop_type)
    if result:
        for algo, errors in result.items():
            total_results[algo].append(errors['total'])

print("\nAverage total error per algorithm (lower is better):\n")
for algo in best_results.keys():
    if total_results[algo]:
        avg_error = statistics.mean(total_results[algo])
        max_error = max(total_results[algo])
        print(f"  {algo:15s}: avg={avg_error:8.4f}  max={max_error:8.4f}")

print("\n" + "=" * 90)
print("FINDINGS")
print("=" * 90)

# Determine best algorithm
best_algo = min(best_results.keys(), key=lambda x: statistics.mean(total_results[x]) if total_results[x] else float('inf'))
best_error = statistics.mean(total_results[best_algo]) if total_results[best_algo] else 0

print(f"\n✓ Best match algorithm: {best_algo}")
print(f"  Average error: {best_error:.4f} units\n")

if best_error < 0.01:
    print("✓ Excellent match - files use the expected algorithm")
elif best_error < 0.1:
    print("⚠️  Reasonable match - minor floating point differences only")
elif best_error < 0.5:
    print("🟡 Moderate differences - possible algorithm mismatch or version change")
else:
    print("🔴 Poor match - handles may be calculated with different algorithm")
    print("\nPOSSIBLE ISSUES:")
    print("  1. Handles were calculated by different tool or algorithm")
    print("  2. Different scale factors used (not 1/3)")
    print("  3. Different tangent calculation method")
    print("  4. Data corruption or modification after export")
