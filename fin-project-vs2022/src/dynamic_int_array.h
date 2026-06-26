#ifndef DYNAMIC_INT_ARRAY_H
#define DYNAMIC_INT_ARRAY_H

/// <summary>
/// Growable array of ints (doubling capacity). Used for task dependency lists,
/// member-id rosters, etc. "Sorted" instances must only be mutated with the
/// sort-preserving ops so dia_find_index (bsearch) stays valid.
/// </summary>
typedef struct {
    int *data;
    int  count;
    int  capacity;
} DynamicIntArray;

/// <summary>Initialise an empty array (no allocation yet).</summary>
/// <param name="a">Array to initialise.</param>
void dia_init(DynamicIntArray *a);

/// <summary>Free the backing buffer and reset to empty.</summary>
/// <param name="a">Array to free.</param>
void dia_free(DynamicIntArray *a);

/// <summary>Append a value at the end, growing capacity if needed. Does NOT keep order.</summary>
/// <param name="a">Array.</param>
/// <param name="value">Value to append.</param>
/// <returns>1 on success, 0 on allocation failure.</returns>
int  dia_append(DynamicIntArray *a, int value);

/// <summary>Remove the first occurrence by swapping in the last element (O(1), NOT order-preserving).</summary>
/// <param name="a">Array.</param>
/// <param name="value">Value to remove.</param>
/// <returns>1 if found and removed, 0 otherwise.</returns>
int  dia_remove(DynamicIntArray *a, int value);

/// <summary>Order-preserving remove for a sorted array: shift the tail down so order (and bsearch) stay valid.</summary>
/// <param name="a">Sorted array.</param>
/// <param name="value">Value to remove.</param>
/// <returns>1 if found and removed, 0 otherwise.</returns>
int  dia_sort_remove(DynamicIntArray *a, int value);

/// <summary>Insert a value at a given index, shifting later elements right.</summary>
/// <param name="a">Array.</param>
/// <param name="index">Target index (clamped/appended if out of range).</param>
/// <param name="value">Value to insert.</param>
/// <returns>1 on success, 0 on allocation failure / bad index.</returns>
int  dia_insert(DynamicIntArray *a, int index, int value);

/// <summary>Insert into its ascending-sorted position (assumes the array is already sorted).</summary>
/// <param name="a">Sorted array.</param>
/// <param name="value">Value to insert.</param>
/// <returns>1 on success, 0 on failure.</returns>
int  dia_sort_insert(DynamicIntArray *a, int value);

/// <summary>Binary-search for a value. REQUIRES the array to be sorted ascending.</summary>
/// <param name="a">Sorted array.</param>
/// <param name="value">Value to find.</param>
/// <returns>Index of the value, or -1 if not present.</returns>
int  dia_find_index(const DynamicIntArray *a, int value);

#endif /* DYNAMIC_INT_ARRAY_H */
