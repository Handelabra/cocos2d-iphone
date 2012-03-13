/* Copyright (c) 2007 Scott Lembcke
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/** 
 @file
 arrd on Chipmunk cpArray.
 ccArray is a faster alternative to NSMutableArray, it does pretty much the
 same thing (stores NSObjects and retains/releases them appropriately). It's
 faster because:
 - it uses a plain C interface so it doesn't incur Objective-c messaging overhead 
 - it assumes you know what you're doing, so it doesn't spend time on safety checks
 (index out of bounds, required capacity etc.)
 - comparisons are done using pointer equality instead of isEqual
 
 There are 2 kind of functions:
 - ccArray functions that manipulates objective-c objects (retain and release are performanced)
 - ccCArray functions that manipulates values like if they were standard C structures (no retain/release is performed)
 */

#ifndef CC_ARRAY_H
#define CC_ARRAY_H

#import <Foundation/Foundation.h>

#import <stdlib.h>
#import <string.h>

#import "../ccMacros.h"


#pragma mark -
#pragma mark ccArray for Objects

// Easy integration	
#define CCARRAYDATA_FOREACH(__array__, __object__)															\
__object__=__array__->arr[0]; for(NSUInteger i=0, num=__array__->num; i<num; i++, __object__=__array__->arr[i])	\

#if defined(__has_feature) && __has_feature(objc_arc)
	typedef __strong id CCARRAY_ID;
#else
	typedef id CCARRAY_ID;
#endif

typedef struct ccArray {
	NSUInteger num, max;
	CCARRAY_ID *arr;
} ccArray;

/** Allocates and initializes a new array with specified capacity */
static inline ccArray* ccArrayNew(NSUInteger capacity) {
	if (capacity == 0)
		capacity = 1; 
	
	ccArray *arr = (ccArray*)malloc( sizeof(ccArray) );
	arr->num = 0;
	arr->arr =  (CCARRAY_ID *)calloc(capacity, sizeof(id));
	arr->max = capacity;
	
	return arr;
}

inline void ccArrayRemoveAllObjects(ccArray *arr);

/** Frees array after removing all remaining objects. Silently ignores nil arr. */
inline void ccArrayFree(ccArray *arr);

/** Doubles array capacity */
static inline void ccArrayDoubleCapacity(ccArray *arr)
{
	arr->max *= 2;
	CCARRAY_ID *newArr = (CCARRAY_ID *)realloc( arr->arr, arr->max * sizeof(id) );
	// will fail when there's not enough memory
    NSCAssert(newArr != NULL, @"ccArrayDoubleCapacity failed. Not enough memory");
	arr->arr = newArr;
}

/** Increases array capacity such that max >= num + extra. */
inline void ccArrayEnsureExtraCapacity(ccArray *arr, NSUInteger extra);

/** shrinks the array so the memory footprint corresponds with the number of items */
static inline void ccArrayShrink(ccArray *arr)
{
    NSUInteger newSize;
	
	//only resize when necessary
	if (arr->max > arr->num && !(arr->num==0 && arr->max==1))
	{
		if (arr->num!=0) 
		{
			newSize=arr->num;
			arr->max=arr->num; 
		}
		else 
		{//minimum capacity of 1, with 0 elements the array would be free'd by realloc
			newSize=1;
			arr->max=1;
		}
		
		arr->arr = (CCARRAY_ID *) realloc(arr->arr,newSize * sizeof(id) );
		NSCAssert(arr->arr!=NULL,@"could not reallocate the memory");
	}
} 

/** Returns index of first occurence of object, NSNotFound if object not found. */
inline NSUInteger ccArrayGetIndexOfObject(ccArray *arr, id object);

/** Returns a Boolean value that indicates whether object is present in array. */
inline BOOL ccArrayContainsObject(ccArray *arr, id object);

/** Appends an object. Bahaviour undefined if array doesn't have enough capacity. */
static inline void ccArrayAppendObject(ccArray *arr, id object)
{
	arr->arr[arr->num] = CC_ARC_RETAIN(object);
	arr->num++;
}

/** Appends an object. Capacity of arr is increased if needed. */
inline void ccArrayAppendObjectWithResize(ccArray *arr, id object);

/** Appends objects from plusArr to arr. Behaviour undefined if arr doesn't have
 enough capacity. */
inline void ccArrayAppendArray(ccArray *arr, ccArray *plusArr);

/** Appends objects from plusArr to arr. Capacity of arr is increased if needed. */
inline void ccArrayAppendArrayWithResize(ccArray *arr, ccArray *plusArr);

/** Inserts an object at index */
static inline void ccArrayInsertObjectAtIndex(ccArray *arr, id object, NSUInteger index)
{
	NSCAssert(index<=arr->num, @"Invalid index. Out of bounds");
	
	ccArrayEnsureExtraCapacity(arr, 1);
	
	NSUInteger remaining = arr->num - index;
	if( remaining > 0)
		memmove((void *)&arr->arr[index+1], (void *)&arr->arr[index], sizeof(id) * remaining );
	
	arr->arr[index] = CC_ARC_RETAIN(object);
	arr->num++;
}

/** Swaps two objects */
inline void ccArraySwapObjectsAtIndexes(ccArray *arr, NSUInteger index1, NSUInteger index2);

/** Removes all objects from arr */
static inline void ccArrayRemoveAllObjects(ccArray *arr)
{
	while( arr->num > 0 )
		CC_ARC_RELEASE(arr->arr[--arr->num]);
}

/** Removes object at specified index and pushes back all subsequent objects.
 Behaviour undefined if index outside [0, num-1]. */
static inline void ccArrayRemoveObjectAtIndex(ccArray *arr, NSUInteger index)
{
	CC_ARC_RELEASE(arr->arr[index]);
	arr->num--;
	
	NSUInteger remaining = arr->num - index;
	if(remaining>0)
		memmove((void *)&arr->arr[index], (void *)&arr->arr[index+1], remaining * sizeof(id));
}

/** Removes object at specified index and fills the gap with the last object,
 thereby avoiding the need to push back subsequent objects.
 Behaviour undefined if index outside [0, num-1]. */
static inline void ccArrayFastRemoveObjectAtIndex(ccArray *arr, NSUInteger index)
{
	CC_ARC_RELEASE(arr->arr[index]);
	NSUInteger last = --arr->num;
	arr->arr[index] = arr->arr[last];
}

inline void ccArrayFastRemoveObject(ccArray *arr, id object);

/** Searches for the first occurance of object and removes it. If object is not
 found the function has no effect. */
inline void ccArrayRemoveObject(ccArray *arr, id object);

/** Removes from arr all objects in minusArr. For each object in minusArr, the
 first matching instance in arr will be removed. */
inline void ccArrayRemoveArray(ccArray *arr, ccArray *minusArr);

/** Removes from arr all objects in minusArr. For each object in minusArr, all
 matching instances in arr will be removed. */
static inline void ccArrayFullRemoveArray(ccArray *arr, ccArray *minusArr)
{
	NSUInteger back = 0;
	
	for( NSUInteger i = 0; i < arr->num; i++) {
		if( ccArrayContainsObject(minusArr, arr->arr[i]) ) {
			CC_ARC_RELEASE(arr->arr[i]);
			back++;
		} else
			arr->arr[i - back] = arr->arr[i];
	}
	
	arr->num -= back;
}

/** Sends to each object in arr the message identified by given selector. */
static inline void ccArrayMakeObjectsPerformSelector(ccArray *arr, SEL sel)
{
	for( NSUInteger i = 0; i < arr->num; i++)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
		[arr->arr[i] performSelector:sel];
#pragma clang diagnostic pop
}

static inline void ccArrayMakeObjectsPerformSelectorWithObject(ccArray *arr, SEL sel, id object)
{
	for( NSUInteger i = 0; i < arr->num; i++)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
		[arr->arr[i] performSelector:sel withObject:object];
#pragma clang diagnostic pop
}

static inline void ccArrayMakeObjectPerformSelectorWithArrayObjects(ccArray *arr, SEL sel, id object)
{
	for( NSUInteger i = 0; i < arr->num; i++)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
		[object performSelector:sel withObject:arr->arr[i]];
#pragma clang diagnostic pop
}


#pragma mark -
#pragma mark ccCArray for Values (c structures)

typedef ccArray ccCArray;

inline void ccCArrayRemoveAllValues(ccCArray *arr);

/** Allocates and initializes a new C array with specified capacity */
static inline ccCArray* ccCArrayNew(NSUInteger capacity) {
	if (capacity == 0)
		capacity = 1; 
	
	ccCArray *arr = (ccCArray*)malloc( sizeof(ccCArray) );
	arr->num = 0;
	arr->arr =  (CCARRAY_ID *) malloc( capacity * sizeof(id) );
	arr->max = capacity;
	
	return arr;
}

/** Frees C array after removing all remaining values. Silently ignores nil arr. */
inline void ccCArrayFree(ccCArray *arr);

/** Doubles C array capacity */
inline void ccCArrayDoubleCapacity(ccCArray *arr);

/** Increases array capacity such that max >= num + extra. */
inline void ccCArrayEnsureExtraCapacity(ccCArray *arr, NSUInteger extra);

/** Returns index of first occurence of value, NSNotFound if value not found. */
static inline NSUInteger ccCArrayGetIndexOfValue(ccCArray *arr, CCARRAY_ID value)
{
	for( NSUInteger i = 0; i < arr->num; i++)
		if( [arr->arr[i] isEqual:value] ) return i;
	return NSNotFound;
}

/** Returns a Boolean value that indicates whether value is present in the C array. */
static inline BOOL ccCArrayContainsValue(ccCArray *arr, CCARRAY_ID value)
{
	return ccCArrayGetIndexOfValue(arr, value) != NSNotFound;
}

/** Inserts a value at a certain position. Behaviour undefined if aray doesn't have enough capacity */
static inline void ccCArrayInsertValueAtIndex( ccCArray *arr, CCARRAY_ID value, NSUInteger index)
{
	NSCAssert( index < arr->max, @"ccCArrayInsertValueAtIndex: invalid index");
	
	NSUInteger remaining = arr->num - index;
	
	// last Value doesn't need to be moved
	if( remaining > 0) {
		// tex coordinates
		memmove((void *)&arr->arr[index+1], (void *)&arr->arr[index], sizeof(void*) * remaining );
	}
	
	arr->num++;	
	arr->arr[index] = value;
}

/** Appends an value. Bahaviour undefined if array doesn't have enough capacity. */
static inline void ccCArrayAppendValue(ccCArray *arr, CCARRAY_ID value)
{
	arr->arr[arr->num] = (id) value;
	arr->num++;
}

/** Appends an value. Capacity of arr is increased if needed. */
static inline void ccCArrayAppendValueWithResize(ccCArray *arr, CCARRAY_ID value)
{
	ccCArrayEnsureExtraCapacity(arr, 1);
	ccCArrayAppendValue(arr, value);
}

/** Appends values from plusArr to arr. Behaviour undefined if arr doesn't have
 enough capacity. */
inline void ccCArrayAppendArray(ccCArray *arr, ccCArray *plusArr);

/** Appends values from plusArr to arr. Capacity of arr is increased if needed. */
inline void ccCArrayAppendArrayWithResize(ccCArray *arr, ccCArray *plusArr);

/** Removes all values from arr */
inline void ccCArrayRemoveAllValues(ccCArray *arr);

/** Removes value at specified index and pushes back all subsequent values.
 Behaviour undefined if index outside [0, num-1].
 @since v0.99.4
 */
inline void ccCArrayRemoveValueAtIndex(ccCArray *arr, NSUInteger index);

/** Removes value at specified index and fills the gap with the last value,
 thereby avoiding the need to push back subsequent values.
 Behaviour undefined if index outside [0, num-1].
 @since v0.99.4
 */
inline void ccCArrayFastRemoveValueAtIndex(ccCArray *arr, NSUInteger index);

/** Searches for the first occurance of value and removes it. If value is not found the function has no effect.
 @since v0.99.4
 */
static inline void ccCArrayRemoveValue(ccCArray *arr, CCARRAY_ID value)
{
	NSUInteger index = ccCArrayGetIndexOfValue(arr, value);
	if (index != NSNotFound)
		ccCArrayRemoveValueAtIndex(arr, index);
}

/** Removes from arr all values in minusArr. For each Value in minusArr, the first matching instance in arr will be removed.
 @since v0.99.4
 */
inline void ccCArrayRemoveArray(ccCArray *arr, ccCArray *minusArr);

/** Removes from arr all values in minusArr. For each value in minusArr, all matching instances in arr will be removed.
 @since v0.99.4
 */
static inline void ccCArrayFullRemoveArray(ccCArray *arr, ccCArray *minusArr)
{
	NSUInteger back = 0;
	
	for( NSUInteger i = 0; i < arr->num; i++) {
		if( ccCArrayContainsValue(minusArr, arr->arr[i]) ) {
			back++;
		} else
			arr->arr[i - back] = arr->arr[i];
	}
	
	arr->num -= back;
}

//used by mergesortL
static inline void memswp(void* a, void* b, size_t width)
{
	if (width == sizeof(void*)) {
        // Optimization for pointer sized swap:
        void* tmp;
        tmp = *(void**)a;
        *(void**)a = *(void**)b;
        *(void**)b = tmp;
        return;
    }
	// default uses memcpy:
	char tmp[width];
	memcpy(tmp, a, width);
	memcpy(a, b, width);
	memcpy(b, tmp, width);
}


// iterative mergesort arrd on
//  http://www.inf.fh-flensburg.de/lang/algorithmen/sortieren/merge/mergiter.htm  

static inline int mergesortL(ccCArray* array, size_t width, int (*compar)(const void *, const void *))
{
	NSInteger h, i, j, k, l, m, n = array->num;
    CCARRAY_ID *arr = array->arr; 
	CCARRAY_ID *A; // points to an element
	//id B = NSZoneMalloc(NULL,(n/2 + 1) * width); // points to a temp array
    CCARRAY_ID *B = (CCARRAY_ID*) malloc(n * width);
    
	for (h = 1; h < n; h += h) {
        for (m = n - 1 - h; m >= 0; m -= h + h) {
            l = m - h + 1;
            if (l < 0)
                l = 0;
            
            // Copy first half of the array into helper B:
            j = m+1;
            memcpy(B, arr + (l * width), (j-l) * width);
            
            for (i = 0, k = l; k < j && j <= m + h; k++) {
                A = arr + (width * j); // A = [self objectAtIndex:j];
                if (compar(A, B + (i * width)) > 0) {
                    memswp(arr+(k*width), B+(i*width), width); i+=1;
                } else {
                    memswp(arr+(k*width), A, width); j+=1;
                }
            }
            
            while (k < j) // This loop could be optimized
                memswp(arr+(k++*width), B+(i++*width), width);
        }
	}
    
	free(B);
	return 0;
}

static inline void insertionSort(ccCArray* arr, int (*comparator)(const void *, const void *))
{
    // It sorts source array in ascending order
	
	// adaptive - performance adapts to the initial order of elements
	// stable - insertion sort retains relative order of the same elements
	// in-place - requires constant amount of additional space
	// online - new elements can be added during the sort
	
	NSInteger i,j,length = arr->num;
	
	CCARRAY_ID *x = arr->arr;
	id temp;	
    
	// insertion sort
	for(i=1; i<length; i++)
	{
		j = i;
		//continue moving element downwards while order is descending 
		while( j>0 && ( comparator(  &x[j-1],  &x[j]  ) == NSOrderedDescending) )
		{
			temp = x[j];
			x[j] = x[j-1];
			x[j-1] = temp;
			j--;
		}
	}
}

#endif // CC_ARRAY_H
