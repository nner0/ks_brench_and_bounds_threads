#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>

struct Node {
    int level, value, weight;
    double bound;
};

struct Item {
    int value, weight;
    double ratio;
};

bool cmp(const Item &a, const Item &b) {
    return a.ratio > b.ratio;
}

double bound(const Node &u, int n, int W, const std::vector<Item> &items) {
    if (u.weight >= W) return 0;

    double profit_bound = u.value;
    int j = u.level + 1;
    int total_weight = u.weight;

    while (j < n && total_weight + items[j].weight <= W) {
        total_weight += items[j].weight;
        profit_bound += items[j].value;
        j++;
    }

    if (j < n)
        profit_bound += (double)(W - total_weight) * items[j].ratio;

    return profit_bound;
}

struct CompareBound {
    bool operator()(const Node &a, const Node &b) {
        return a.bound < b.bound;
    }
};

std::priority_queue<Node, std::vector<Node>, CompareBound> PQ;
std::mutex pq_mutex;
std::mutex result_mutex;
std::condition_variable cv;
bool done = false;
int global_max_profit = 0;
int global_total_weight = 0;

void worker(int n, int W, const std::vector<Item>& items) {
    while (true) {
        Node u;

        {
            std::unique_lock<std::mutex> lock(pq_mutex);
            cv.wait(lock, [] { return !PQ.empty() || done; });

            if (done && PQ.empty())
                return;

            u = PQ.top();
            PQ.pop();
        }

        if (u.level + 1 >= n) continue;

        Node v;
        v.level = u.level + 1;

        // Ветка включения
        v.weight = u.weight + items[v.level].weight;
        v.value = u.value + items[v.level].value;

        if (v.weight <= W) {
            std::lock_guard<std::mutex> lock(result_mutex);
            if (v.value > global_max_profit) {
                global_max_profit = v.value;
                global_total_weight = v.weight;
            }
        }

        v.bound = bound(v, n, W, items);
        if (v.bound > global_max_profit) {
            std::lock_guard<std::mutex> lock(pq_mutex);
            PQ.push(v);
            cv.notify_all();
        }

        v.weight = u.weight;
        v.value = u.value;
        v.bound = bound(v, n, W, items);

        if (v.bound > global_max_profit) {
            std::lock_guard<std::mutex> lock(pq_mutex);
            PQ.push(v);
            cv.notify_all();
        }
    }
}

std::pair<int, int> knapsack(int W, std::vector<Item>& items, int n, int thread_count = 4) {
    std::sort(items.begin(), items.end(), cmp);

    Node u;
    u.level = -1;
    u.value = u.weight = 0;
    u.bound = bound(u, n, W, items);

    {
        std::lock_guard<std::mutex> lock(pq_mutex);
        PQ.push(u);
    }

    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(worker, n, W, std::ref(items));
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lock(pq_mutex);
        if (PQ.empty())
            break;
    }

    {
        std::lock_guard<std::mutex> lock(pq_mutex);
        done = true;
        cv.notify_all();
    }

    for (auto& t : threads) t.join();

    return {global_max_profit, global_total_weight};
}

int main() {
    std::ifstream input("../ks_19_0");
    if (!input) {
        std::cerr << "Error opening file!" << std::endl;
        return 1;
    }

    int N, W;
    input >> N >> W;
    std::vector<Item> items(N);

    for (int i = 0; i < N; ++i) {
        input >> items[i].value >> items[i].weight;
        items[i].ratio = (double)items[i].value / items[i].weight;
    }

    input.close();

    if (N == 0 || W == 0) {
        std::cout << "Maximum profit: 0\nTotal weight: 0\n";
        return 0;
    }

    auto result = knapsack(W, items, N, std::thread::hardware_concurrency());

    std::cout << "Maximum profit: " << result.first << std::endl;
    std::cout << "Total weight: " << result.second << std::endl;

    return 0;
}
