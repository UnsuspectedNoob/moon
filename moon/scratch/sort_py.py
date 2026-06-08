import sys
sys.setrecursionlimit(2000000)

def sorted_list(lst):
    if len(lst) < 2:
        return lst
    
    mid = len(lst) // 2
    left = sorted_list(lst[:mid])
    right = sorted_list(lst[mid:])
    
    return merge(left, right)

def merge(left, right):
    total = []
    i = 0
    j = 0
    while i < len(left) and j < len(right):
        if left[i] < right[j]:
            total.append(left[i])
            i += 1
        else:
            total.append(right[j])
            j += 1
            
    if i < len(left):
        total.extend(left[i:])
    if j < len(right):
        total.extend(right[j:])
        
    return total

a = list(range(1000000, 0, -1))
sorted_list(a)
