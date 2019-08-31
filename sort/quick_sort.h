
/*
 * Copyright (C) Jianyong Chen
 */

#ifndef QUICK_SORT_H__
#define QUICK_SORT_H__


#include <vector>


class QuickSorter
{
    public:
        void Sort(std::vector<int> &);

    private:
        void sort(std::vector<int> &, int, int);
        void insertion_sort(std::vector<int> &, int, int);
        int median3(std::vector<int> &, int, int);
};


#endif /* QUICK_SORT_H__ */
