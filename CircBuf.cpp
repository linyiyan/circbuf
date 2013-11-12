/***************************************************************************************
*
* Author : Yiyan Lin
*
* #1. An dynamic increasing array-based FIFO circular buffer implementation.
*
* #2. And a application using circular buffer, count hit() function call in last 5sec.
*     -- Assume hit() may be called every 1 milisecond.
*
****************************************************************************************/

#include<iostream>
using std::cout; using std::cin; using std::endl;
using std::ostream;

#include<string>
using std::string;

#include <initializer_list>
using std::initializer_list;

#include <limits>
using std::numeric_limits;

#include <sys/timeb.h>

#include <sstream>
using std::ostringstream;

#include <algorithm>
using std::transform;

#include <iterator>
using std::ostream_iterator;

#include<random>
using std::default_random_engine;
using std::uniform_int_distribution;

#include <chrono>
#include <thread>
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

#include <cassert>

struct BufEntry{
	long long int timestamp;
	BufEntry(){ timestamp = numeric_limits<long long int>::min(); }
	BufEntry(long long int ts) : timestamp(ts){}
	bool operator == (const BufEntry& entry) { return this->timestamp == entry.timestamp; }
	string to_string(){
		ostringstream oss;
		oss << timestamp;
		return oss.str();
	}
};

template<class T>
class CircBuf{
private:
	size_t size_;  /* size of the buffer */
	size_t count_; /* count of buffer entry */
	size_t front_; /* front index of entry, next position to read */
	size_t back_;  /* back index of entry, next position to write */
	T* buffer_;    /* data */

public:

	/* default constructor, default size equal to 10 */
	CircBuf(size_t s = 10) : size_(s), front_(0), count_(0), back_(0) {
		buffer_ = new T[s];
		for (size_t i = 0; i < s; i++) buffer_[i] = T();
	}

	/* constructor using initializer list */
	CircBuf(initializer_list<T> l) : size_(l.size()) , front_(0) , count_(0) , back_(0){
		buffer_ = new T[size_];
		for (auto ptr = l.begin(); ptr != l.end(); ptr++){
			buffer_[back_++] = *ptr;
			count_++;
		}
	}

	/* copy constructor */
	CircBuf(const CircBuf& buf){
		this->size_ = buf.size_;
		this->count_ = buf.count_;
		this->front_ = buf.front_;
		this->back_ = buf.back_;

		this->buffer_ = new T[this->size_];
		for (size_t i = 0; i < buf.size_; i++)
			this->buffer_[i] = buf.buffer_[i];
	}

	/* assignment operator */
	CircBuf& operator=(const CircBuf& buf){
		if (this == &buf) return *this; // protect against invalid self-assignment

		this->size_ = buf.size_;
		this->count_ = buf.count_;
		this->front_ = buf.front_;
		this->back_ = buf.back_;

		// release older data
		delete[] this->buffer_;

		// assign new data
		this->buffer_ = new T[this->size_];
		for (size_t i = 0; i < buf.size_; i++)
			this->buffer_[i] = buf.buffer_[i];

		return *this;
	}

	/* destructor */
	~CircBuf() {
		delete[] buffer_;
	}

	/* remove the first element */
	T remove(){
		if (count_ == 0) return T();
		else{
			count_ -= 1;
			T ret_val = buffer_[front_];
			buffer_[front_] = T();
			front_ = (front_ + 1)%size_;
			return ret_val;
		}
	}

	/* add new  element to last position */
	void add(T elem) {
		if (count_ == size_) { // if space is not enough, double it
			T* newbuffer_ = new T[size_ * 2];
			int j = front_;
			for (size_t i = 0; i < size_ * 2; i++, j = (j + 1)%size_){
				if (i < size_) newbuffer_[i] = buffer_[j];
				else newbuffer_[i] = T();
			}
			front_ = 0;
			back_ = size_;
			size_ = size_ * 2;
			delete[] buffer_;
			buffer_ = newbuffer_;
		}
		buffer_[back_] = elem;
		count_ += 1;
		back_ = (back_ + 1) % size_;

	}

	/* overloaded == operator */
	bool operator==(const CircBuf<T>& cbuf){
		if (this->count_ != cbuf.count_) return false;

		size_t i = this->front_, j = cbuf.front_;
		for (; i != this->back_ && j != cbuf.back_; i = (i + 1)%this->size_, j = (j + 1)%this->size_){
			if (!(this->buffer_[i] == cbuf.buffer_[j])) return false;
		}
		return true;
	}

	size_t size(){ return size_; }
	size_t first_index(){ return front_; }
	size_t last_index() { return back_; }
	bool empty(){ return count_ == 0; }
	bool full(){ return count_ == size_; }
	const T& operator[] (size_t i) const{return buffer_[i]; } /* support read only index access */

	friend ostream& operator<<(ostream &out, const CircBuf<T> &cb);

};

/* cout support */
ostream& operator<<(ostream &out, const CircBuf<BufEntry> &cb){
	ostringstream oss;
	oss << "CircBuff(" << "Front" << cb.front_ << ", Back:" << cb.back_ << ", Cnt:" << cb.count_ << endl;
	std::transform(cb.buffer_, cb.buffer_ + cb.size_, ostream_iterator<string>(oss, " , "), [](BufEntry& entry){ return entry.to_string(); });
	string res = oss.str();
	res = res.substr(0, res.find_last_of(','));
	out << res << ")" << endl;
	return out;
}

ostream& operator<<(ostream &out, const CircBuf<long> &cb){
	out << "DUMP Front:" << cb.front_ << ", Back:" << cb.back_ << ", Cnt:" << cb.count_ << endl;
	for (size_t i = 0; i < cb.size_; i++)
		out << cb.buffer_[i] << ", ";
	out << endl;
	return out;
}

CircBuf<BufEntry> gcb(5000); /* buffer with 5000 slots */

void hit(){
	_timeb timebuffer;
	_ftime_s(&timebuffer);
	long long int curMili = timebuffer.time * 1000;
	gcb.add(BufEntry(curMili));
}

long getHits(){
	_timeb timebuffer;
	_ftime_s(&timebuffer);
	long long int curMili = timebuffer.time * 1000;

	long count = 0;
	size_t index = gcb.last_index();
	for (int i = 0; i < 5000 && index != 0; i++){
		if (curMili - gcb[index].timestamp > 5000 // if the item is 5 sec ago 
			&& gcb[index].timestamp == std::numeric_limits<long long int>::min() // if current slot has not been s
			) {
				break;
		}
		else {
			count += 1;
			if (index == gcb.first_index()) break;
			else index = index == 0 ? gcb.size() - 1 : index - 1;
		}
	}

	return count;
}

int main(){
	//===========================================
	// Test #1, general circular buffer test
	//===========================================

	const size_t sz = 4;
	CircBuf<long> cb(sz);

	// Add 2 elements
	cb.add(1);
	cb.add(2);

	CircBuf<long> cb1 = {1,2};
	assert(cb == cb1);

	// Remove 2 elements
	cb.remove();
	cb.remove();

	CircBuf<long> cb2 = {};
	assert(cb == cb2);

	// Add 4 elements
	for (long i = 0; i < 4; i++)
		cb.add(i + 27);
	CircBuf<long> cb3 = {27,28,29,30};
	assert(cb==cb3);

	// Remove 4 elements;
	while (!cb.empty())
		cout << cb.remove() << ", ";
	CircBuf<long> cb4 = {};
	assert(cb == cb4);


	//============================================
	// Test #2, hit counts
	//============================================
	_timeb timebuffer;
	_ftime_s(&timebuffer);

	default_random_engine reng;
	reng.seed(timebuffer.time);
	uniform_int_distribution<long> dist(0, 99999);

	int count = 0;
	for (int i = 0; i < 5000; i++){
		std::chrono::milliseconds dura(1);
		std::this_thread::sleep_for(dura);

		long rnum = dist(reng);
		if (rnum % 2 == 0) {
			hit();
			count += 1;
		}
	}

	assert(count == getHits());
}