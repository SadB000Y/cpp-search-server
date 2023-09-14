#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    auto result = temp_server_.FindTopDocuments(raw_query, status);
    if (result.size() > 0) {
        requests_.push_back({ raw_query, 1 });
    }
    else {
        requests_.push_back({ raw_query,0 });
    }
    if (requests_.size() > min_in_day_) {
        CutDeque();
    }
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    auto result = temp_server_.FindTopDocuments(raw_query);
    if (result.size() > 0) {
        requests_.push_back({ raw_query, 1 });
    }
    else {
        requests_.push_back({ raw_query, 0 });
    }
    if (requests_.size() > min_in_day_) {
        CutDeque();
    }
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    int count = 0;
    for (auto memb : requests_) {
        if (memb.sucsess == 0) {
            count++;
        }
    }
    return count;
}

void RequestQueue::CutDeque() {
    while (requests_.size() != min_in_day_) {
        requests_.pop_front();
    }
}

