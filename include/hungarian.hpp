// hungarian.hpp - single-header Kuhn–Munkres solver (square matrix), MIT.
#pragma once
#include <vector>
#include <limits>
#include <algorithm>

inline int hungarian(const std::vector<std::vector<double>>& cost,
                     std::vector<int>& rowsol,
                     double& total_cost)
{
    int n = cost.size();
    rowsol.assign(n, -1);
    std::vector<double> u(n+1), v(n+1);
    std::vector<int> p(n+1), way(n+1);

    for(int i=1;i<=n;++i){
        p[0] = i;
        int j0 = 0;
        std::vector<double> minv(n+1, std::numeric_limits<double>::infinity());
        std::vector<char> used(n+1, false);
        do{
            used[j0] = true;
            int i0 = p[j0], j1 = 0;
            double delta = std::numeric_limits<double>::infinity();
            for(int j=1;j<=n;++j) if(!used[j]){
                double cur = cost[i0-1][j-1] - u[i0] - v[j];
                if(cur < minv[j]) { minv[j] = cur; way[j] = j0; }
                if(minv[j] < delta) { delta = minv[j]; j1 = j; }
            }
            for(int j=0;j<=n;++j){
                if(used[j]) { u[p[j]] += delta; v[j] -= delta; }
                else { minv[j] -= delta; }
            }
            j0 = j1;
        } while(p[j0] != 0);
        do{
            int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while(j0);
    }
    rowsol.assign(n, -1);
    for(int j=1;j<=n;++j) if(p[j]) rowsol[p[j]-1] = j-1;
    total_cost = -v[0];
    return 0;
}
