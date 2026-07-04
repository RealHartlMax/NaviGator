#!/usr/bin/env python3
"""Test different tangent calculation methods to find the exact algorithm"""

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

def test_tangent_algorithm(points, loop_type, algorithm_name, algorithm_func):
    """Test a tangent calculation algorithm"""
    is_closed = loop_type == 'close'
    n = len(points)
    SCALE = 1.0 / 3.0
    
    total_error = 0
    error_count = 0
    
    for i in range(min(100, n)):
        point = points[i]
        pos = point['pos']
        handleA_actual = point['handleA']
        handleB_actual = point['handleB']
        
        prev_idx = (i - 1) % n if is_closed else (i - 1 if i > 0 else None)
        next_idx = (i + 1) % n if is_closed else (i + 1 if i < n - 1 else None)
        
        prev_pos = points[prev_idx]['pos'] if prev_idx is not None else None
        next_pos = points[next_idx]['pos'] if next_idx is not None else None
        
        if not (prev_pos and next_pos):
            continue
        
        # Use algorithm to get tangent
        tangent = algorithm_func(pos, prev_pos, next_pos)
        handle_dir = vec_normalize(tangent)
        
        prev_dist = vec_distance(pos, prev_pos)
        next_dist = vec_distance(pos, next_pos)
        
        # Calculate expected handles
        handleA_expected = vec_add(pos, vec_scale(handle_dir, -prev_dist * SCALE))
        handleB_expected = vec_add(pos, vec_scale(handle_dir, next_dist * SCALE))
        
        # Calculate error
        error_A = vec_distance(handleA_actual, handleA_expected)
        error_B = vec_distance(handleB_actual, handleB_expected)
        
        total_error += error_A + error_B
        error_count += 1
    
    avg_error = total_error / error_count if error_count > 0 else float('inf')
    return avg_error

# Define different tangent algorithms
def tangent_simple_prev_next(pos, prev_pos, next_pos):
    """Catmull-Rom: tangent = next - prev"""
    return vec_subtract(next_pos, prev_pos)

def tangent_centripetal_catmull_rom(pos, prev_pos, next_pos):
    """Centripetal Catmull-Rom with distance weighting"""
    d_prev = vec_distance(pos, prev_pos)
    d_next = vec_distance(pos, next_pos)
    
    t_prev = d_prev ** 0.5  # sqrt for centripetal
    t_next = d_next ** 0.5
    
    if t_prev + t_next > 0.0001:
        weight = t_next / (t_prev + t_next)
        prev_vec = vec_subtract(pos, prev_pos)
        next_vec = vec_subtract(next_pos, pos)
        return (
            prev_vec[0] * (1 - weight) + next_vec[0] * weight,
            prev_vec[1] * (1 - weight) + next_vec[1] * weight,
            prev_vec[2] * (1 - weight) + next_vec[2] * weight
        )
    return vec_subtract(next_pos, prev_pos)

def tangent_uniform_catmull_rom(pos, prev_pos, next_pos):
    """Uniform Catmull-Rom"""
    return vec_subtract(next_pos, prev_pos)

def tangent_barry_goldman(pos, prev_pos, next_pos):
    """Barry-Goldman algorithm"""
    d_prev = vec_distance(pos, prev_pos)
    d_next = vec_distance(pos, next_pos)
    
    if d_prev > 0 and d_next > 0:
        # Weighted average of the two segment directions
        dir_prev = vec_normalize(vec_subtract(pos, prev_pos))
        dir_next = vec_normalize(vec_subtract(next_pos, pos))
        
        # Average direction
        avg_dir = (
            (dir_prev[0] + dir_next[0]) / 2,
            (dir_prev[1] + dir_next[1]) / 2,
            (dir_prev[2] + dir_next[2]) / 2
        )
        return vec_scale(avg_dir, (d_prev + d_next) / 2)
    return vec_subtract(next_pos, prev_pos)

def tangent_beier_neely(pos, prev_pos, next_pos):
    """Beier-Neely algorithm"""
    d_prev = vec_distance(pos, prev_pos)
    d_next = vec_distance(pos, next_pos)
    
    if d_prev > 0 and d_next > 0:
        # Weighted by inverse distances
        w_prev = 1.0 / d_prev if d_prev > 0 else 0
        w_next = 1.0 / d_next if d_next > 0 else 0
        
        total_w = w_prev + w_next
        if total_w > 0.0001:
            prev_vec = vec_scale(vec_subtract(pos, prev_pos), w_prev / total_w)
            next_vec = vec_scale(vec_subtract(next_pos, pos), w_next / total_w)
            return (
                prev_vec[0] + next_vec[0],
                prev_vec[1] + next_vec[1],
                prev_vec[2] + next_vec[2]
            )
    return vec_subtract(next_pos, prev_pos)

def tangent_double_scale(pos, prev_pos, next_pos):
    """Use scale factor 1/6 instead of 1/3"""
    # This is equivalent to using different scale in handle calculation
    return vec_subtract(next_pos, prev_pos)

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
print("TESTING DIFFERENT TANGENT/CURVE ALGORITHMS")
print("=" * 90)
print(f"\nFile: trains1.dat | Points: {len(points)} | Type: {loop_type}\n")

algorithms = [
    ("Catmull-Rom (Standard)", tangent_simple_prev_next),
    ("Centripetal Catmull-Rom", tangent_centripetal_catmull_rom),
    ("Barry-Goldman", tangent_barry_goldman),
    ("Beier-Neely", tangent_beier_neely),
]

results = []
for name, func in algorithms:
    error = test_tangent_algorithm(points, loop_type, name, func)
    results.append((name, error))
    print(f"{name:30s}: avg error = {error:8.4f} units")

print("\n" + "=" * 90)
print("BEST MATCH")
print("=" * 90)

best_name, best_error = min(results, key=lambda x: x[1])
print(f"\n✓ Best algorithm: {best_name}")
print(f"  Average error: {best_error:.4f} units")

if best_error < 0.1:
    print("\n✓ EXCELLENT MATCH - Found the algorithm!")
elif best_error < 0.5:
    print("\n⚠️  Good match but not perfect")
elif best_error < 1.0:
    print("\n🟡 Reasonable match - algorithm is close but not exact")
else:
    print("\n🔴 Poor match - algorithm is significantly different")

print(f"""
Note: Even with the best algorithm, we still have {best_error:.2f} units of error.
This could be due to:
1. Floating point rounding in original calculation
2. Different initial precision in stored handles
3. Combination of multiple approximations
4. Numerical differences in sqrt/normalize operations
""")
