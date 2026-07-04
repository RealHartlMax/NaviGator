#!/usr/bin/env python3
"""Detailed analysis of curve handle calculations"""

import math
from pathlib import Path

def parse_curve_point(line):
    """Parse a curve point line: c x y z handleA_x handleA_y handleA_z handleB_x handleB_y handleB_z scalar info"""
    parts = line.strip().split()
    if not parts or parts[0] != 'c':
        return None
    
    try:
        return {
            'pos': (float(parts[1]), float(parts[2]), float(parts[3])),
            'handleA': (float(parts[4]), float(parts[5]), float(parts[6])),
            'handleB': (float(parts[7]), float(parts[8]), float(parts[9])),
            'scalar': float(parts[10]),
            'info': int(parts[11]) if len(parts) > 11 else 0
        }
    except (ValueError, IndexError):
        return None

def parse_linear_point(line):
    """Parse a linear point line: x y z scalar info"""
    parts = line.strip().split()
    if not parts or parts[0] == 'c':
        return None
    
    try:
        return {
            'pos': (float(parts[0]), float(parts[1]), float(parts[2])),
            'scalar': float(parts[3]) if len(parts) > 3 else 0,
            'info': int(parts[4]) if len(parts) > 4 else 0
        }
    except (ValueError, IndexError):
        return None

def vec_distance(p1, p2):
    """Calculate distance between two 3D points"""
    return math.sqrt((p1[0]-p2[0])**2 + (p1[1]-p2[1])**2 + (p1[2]-p2[2])**2)

def vec_subtract(p1, p2):
    """Vector subtraction"""
    return (p1[0]-p2[0], p1[1]-p2[1], p1[2]-p2[2])

def vec_normalize(v):
    """Normalize a vector"""
    length = math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)
    if length < 0.0001:
        return (1.0, 0.0, 0.0)
    return (v[0]/length, v[1]/length, v[2]/length)

def verify_curve_handles(points, loop_type='open'):
    """
    Verify that the curve handles match the algorithm:
    1. Get handle direction from prev->next tangent
    2. Scale by 1/3 * distance
    """
    is_closed = loop_type == 'close'
    CURVE_HANDLE_SCALE = 1.0 / 3.0
    
    issues = []
    
    for i in range(len(points)):
        if 'handleA' not in points[i]:
            continue  # Not a curve point
        
        point = points[i]
        pos = point['pos']
        handleA_actual = point['handleA']
        handleB_actual = point['handleB']
        
        # Get neighbors
        n = len(points)
        if n < 2:
            continue
        
        prev_idx = (i - 1) % n if is_closed else (i - 1 if i > 0 else None)
        next_idx = (i + 1) % n if is_closed else (i + 1 if i < n - 1 else None)
        
        prev_pos = points[prev_idx]['pos'] if prev_idx is not None else None
        next_pos = points[next_idx]['pos'] if next_idx is not None else None
        
        # Calculate expected handle direction
        if prev_pos and next_pos:
            tangent = vec_subtract(next_pos, prev_pos)
        elif next_pos:
            tangent = vec_subtract(next_pos, pos)
        elif prev_pos:
            tangent = vec_subtract(pos, prev_pos)
        else:
            tangent = (1.0, 0.0, 0.0)
        
        handle_dir = vec_normalize(tangent)
        
        # Calculate expected handles
        prev_dist = vec_distance(pos, prev_pos) if prev_pos else 0.0
        next_dist = vec_distance(pos, next_pos) if next_pos else 0.0
        
        if prev_pos:
            expected_handleA = tuple(
                pos[j] - handle_dir[j] * (prev_dist * CURVE_HANDLE_SCALE)
                for j in range(3)
            )
        else:
            expected_handleA = pos
        
        if next_pos:
            expected_handleB = tuple(
                pos[j] + handle_dir[j] * (next_dist * CURVE_HANDLE_SCALE)
                for j in range(3)
            )
        else:
            expected_handleB = pos
        
        # Check if handles match (with tolerance for float precision)
        tolerance = 0.01
        handleA_error = vec_distance(handleA_actual, expected_handleA)
        handleB_error = vec_distance(handleB_actual, expected_handleB)
        
        if handleA_error > tolerance or handleB_error > tolerance:
            issues.append({
                'index': i,
                'pos': pos,
                'handleA_error': handleA_error,
                'handleB_error': handleB_error,
                'expected_A': expected_handleA,
                'actual_A': handleA_actual,
                'expected_B': expected_handleB,
                'actual_B': handleB_actual,
                'has_prev': prev_pos is not None,
                'has_next': next_pos is not None
            })
    
    return issues

# Analyze trains1.dat with curve issues
track_path = Path("G:/dist-debug/Neuer Ordner/trains1.dat")

with open(track_path, 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

# Parse header
header = lines[0].strip().split()
loop_type = header[2]

# Parse all points
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

# Verify handles
issues = verify_curve_handles(points, loop_type)

print("=" * 90)
print(f"CURVE HANDLE VERIFICATION - {track_path.name}")
print(f"Track type: {loop_type} | Total points: {len(points)} | Curve points: {sum(1 for p in points if 'handleA' in p)}")
print("=" * 90)

if not issues:
    print("\n✓ All curve handles match expected algorithm output")
else:
    print(f"\n⚠️  Found {len(issues)} points with potential handle calculation issues\n")
    
    # Show first 5 issues
    for issue in issues[:5]:
        print(f"Point #{issue['index']} at {issue['pos']}")
        print(f"  HandleA error: {issue['handleA_error']:.4f} units")
        if issue['handleA_error'] > 0.01:
            print(f"    Expected: {issue['expected_A']}")
            print(f"    Actual:   {issue['actual_A']}")
        print(f"  HandleB error: {issue['handleB_error']:.4f} units")
        if issue['handleB_error'] > 0.01:
            print(f"    Expected: {issue['expected_B']}")
            print(f"    Actual:   {issue['actual_B']}")
        print(f"  Neighbors: prev={'yes' if issue['has_prev'] else 'no'}, next={'yes' if issue['has_next'] else 'no'}")
        print()

print("=" * 90)
print("ALGORITHM VERIFICATION")
print("=" * 90)
print("""
The curve handle calculation algorithm works as follows:

1. For each curve point, find its neighbors (previous and next point)
   - In closed loops: always have neighbors
   - In open paths: first/last points may not have prev/next

2. Calculate handle direction (tangent):
   - If both prev and next exist: tangent = next - prev
   - Else if next exists: tangent = next - current
   - Else if prev exists: tangent = current - prev
   - Else: default to (1, 0, 0)
   - Result: normalized direction

3. Calculate distances:
   - prev_distance = distance(current, previous)
   - next_distance = distance(current, next)

4. Scale handles by 1/3 of neighboring distances:
   - handleA = current - direction * (prev_distance * 1/3)
   - handleB = current + direction * (next_distance * 1/3)

This is a standard Catmull-Rom / Bézier curve approximation.
""")
