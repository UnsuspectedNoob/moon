import sys
sys.setrecursionlimit(20000)

def quicksort(arr):
    if len(arr) <= 1:
        return arr
    pivot = arr[-1]
    left = []
    right = []
    for i in range(len(arr) - 1):
        if arr[i] < pivot:
            left.append(arr[i])
        else:
            right.append(arr[i])
    return quicksort(left) + [pivot] + quicksort(right)

data = list(range(10000, 0, -1))
sorted_data = quicksort(data)
print(f"Sorted length: {len(sorted_data)}")
