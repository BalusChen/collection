
/*
 * Copyright (C) Jianyong Chen
 */


#include "quick_sort.h"


void
QuickSorter::Sort(std::vector<int> &nums)
{
    sort(nums, 0, nums.size() - 1);
}


void
QuickSorter::sort(std::vector<int> &nums, int left, int right)
{
    if (left + 10 <= right) {
        const int pivot = median3(nums, left, right);

        int  i = left, j = right - 1;
        for ( ;; ) {
            while (nums[++i] <= pivot) {  }
            while (pivot <= nums[--j]) {  }

            if (i < j) {
                std::swap(nums[i], nums[j]);

            } else {
                break;
            }
        }

        std::swap(nums[i], nums[right - 1]);
        sort(nums, left, i - 1);
        sort(nums, i + 1, right);

    } else {
        insertion_sort(nums, left, right);
    }
}


int
QuickSorter::median3(std::vector<int> &nums, int left, int right)
{
    int  center = (left + right) / 2;

    if (nums[right] < nums[left]) {
        std::swap(nums[left], nums[right]);
    }

    if (nums[center] < nums[left]) {
        std::swap(nums[left], nums[center]);
    }

    if (nums[right] < nums[center]) {
        std::swap(nums[center], nums[right]);
    }

    std::swap(nums[center], nums[right - 1]);
    return nums[right - 1];
}


void
QuickSorter::insertion_sort(std::vector<int> &nums, int left, int right)
{
    int  i, j;

    for (i = left + 1; i <= right; i++) {

        for (j = i; j > 0 && nums[j] < nums[j - 1]; j--) {
            std::swap(nums[j], nums[j-1]);
        }
    }
}
