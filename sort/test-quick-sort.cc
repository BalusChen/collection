
/*
 * Copyright (C) Jianyong Chen
 */


#include "quick_sort.h"


static void print_vector(const std::vector<int> &);


int
main(int argc, char **argv)
{
    QuickSorter       sorter;
    std::vector<int>  nums;

    nums = { 3, 5, 7, 1 };
    print_vector(nums);
    sorter.Sort(nums);
    print_vector(nums);

    nums = { 1, 3, 7 };
    print_vector(nums);
    sorter.Sort(nums);
    print_vector(nums);

    nums = { 7, 3, 1 };
    print_vector(nums);
    sorter.Sort(nums);
    print_vector(nums);

    nums = { 17, 37, 51, 3, 17, 13, 23, 11, 7, 3, 27, 19, 49, 51, 79, 3 };
    print_vector(nums);
    sorter.Sort(nums);
    print_vector(nums);

    return 0;
}


static void
print_vector(const std::vector<int> &nums)
{
    for (auto i : nums) {
        printf("%d  ", i);
    }

    printf("\n");
}
