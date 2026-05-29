#include "net/timer_manager.h"

#include <cassert>

namespace web_server {
	namespace net {

		void TimerManager::SiftUp_(size_t i) {
			assert(i < heap_.size());
			size_t parent = (i - 1) / 2;
			// 如果子节点小于父节点（过期时间更早），则向上交换
			while (i > 0 && heap_[i] < heap_[parent]) {
				SwapNode_(i, parent);
				i = parent;
				parent = (i - 1) / 2;
			}
		}

		bool TimerManager::SiftDown_(size_t index, size_t n) {
			assert(index < heap_.size()); 
			assert(n <= heap_.size());
			size_t i = index;
			size_t child = i * 2 + 1; // 左孩子

			while (child < n) {
				// 找出左右孩子中较小的一个
				if (child + 1 < n && heap_[child + 1] < heap_[child]) {
					child++;
				}
				// 如果父节点已经比最小的孩子还小，说明满足最小堆性质，停止下滤
				if (heap_[i] < heap_[child]) {
					break;
				}
				SwapNode_(i, child);
				i = child;
				child = i * 2 + 1;
			}
			return i > index; // 返回是否发生了真正的下滤操作
		}

		void TimerManager::SwapNode_(size_t i, size_t j) {
			assert(i < heap_.size());
			assert(j < heap_.size());
			std::swap(heap_[i], heap_[j]);
			// 交换数组节点的同时，必须同步更新哈希表中的映射关系
			ref_[heap_[i].id] = i;
			ref_[heap_[j].id] = j;
		}

		void TimerManager::Add(int id, int timeout_ms, const TimeoutCallBack& cb) {
			assert(id >= 0);
			size_t i;
			if (ref_.count(id) == 0) {
				// 新增节点：将其放在数组尾部，然后上滤
				i = heap_.size();
				ref_[id] = i;
				heap_.push_back({id, Clock::now() + MS(timeout_ms), cb});
				SiftUp_(i);
			} else {
				// 已存在节点：更新参数并重新调整堆
				i = ref_[id];
				heap_[i].expires = Clock::now() + MS(timeout_ms);
				heap_[i].cb = cb;
				if (!SiftDown_(i, heap_.size())) {
					SiftUp_(i);
				}
			}
		}

		void TimerManager::Adjust(int id, int timeout_ms) {
			assert(!heap_.empty() && ref_.count(id) > 0);
			// 只调整时间，通常只会使过期时间延后，所以通常触发 SiftDown_
			heap_[ref_[id]].expires = Clock::now() + MS(timeout_ms);
			SiftDown_(ref_[id], heap_.size());
		}

		void TimerManager::Del_(size_t index) {
			assert(!heap_.empty() && index < heap_.size());
			// 将要删除的节点与队尾节点交换
			size_t i = index;
			size_t n = heap_.size() - 1;
			assert(i <= n);
			if (i < n) {
				SwapNode_(i, n);
				// 队尾节点换到该位置后，需要重新调整堆
				if (!SiftDown_(i, n)) {
					SiftUp_(i);
				}
			}
			// 删除原来的目标节点（现在已经被换到了队尾）
			ref_.erase(heap_.back().id);
			heap_.pop_back();
		}

		void TimerManager::DoWork(int id) {
			if (heap_.empty() || ref_.count(id) == 0) {
				return;
			}
			size_t i = ref_[id];
			TimerNode node = heap_[i];
			// 触发回调（例如关闭 fd，从 epoll 树中移除等）
			node.cb();
			Del_(i);
		}

		void TimerManager::Tick() {
			// 循环检查堆顶（最近要过期的节点）
			while (!heap_.empty()) {
				TimerNode node = heap_.front();
				// 如果堆顶节点都没过期，说明后面的更不可能过期，直接退出
				if (std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) {
					break; 
				}
				node.cb();
				Del_(0); // 删除堆顶元素
			}
		}

		void TimerManager::Clear() {
			ref_.clear();
			heap_.clear();
		}

		int TimerManager::GetNextTick() {
			Tick(); // 先清理掉已经过期的节点
			int res = -1;
			if (!heap_.empty()) {
				// 计算距离堆顶节点超时还有多少毫秒
				res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
				if (res < 0) {
					res = 0;
				}
			}
			// 这个 res 将直接传递给 EpollPoller::Wait()
			return res;
		}

	}  // namespace net
}  // namespace web_server
