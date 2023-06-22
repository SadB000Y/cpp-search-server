#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double COMPARE = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }
    
    template <typename Doc_Status>
     vector<Document> FindTopDocuments(const string& raw_query,
                                      Doc_Status docstatus) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, docstatus);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < COMPARE) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
    
    vector<Document> FindTopDocuments(const string& raw_query,
                                       DocumentStatus status) const {
            return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus status_, int rating) { return status == status_; });
    }

     vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Entering>
    vector<Document> FindAllDocuments(const Query& query, Entering enter) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                if (enter(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

template <typename Funk>
void RunTestImpl(const string funk_name, Funk funk) {
   funk();
   cerr << funk_name << " ОК"s << endl; 
}

#define RUN_TEST(func) RunTestImpl(#func, func)



void Assert(bool query, const string raw_query, const string file, const string func, unsigned line, const string & hint = " "s) {
    if (query == 0) {
    cout << file << "("s << line << "): "s << func << ": "s;
    cout << "ASSERT("s << raw_query << ") failed.";
    if (!hint.empty()) {
        cout <<" Hint: " << hint; 
    }
    cout << endl;
    abort();
    }

}

#define ASSERT(expr) Assert((expr), (#expr), __FILE__, __FUNCTION__, __LINE__) 
#define ASSERT_HINT(expr, hint) Assert((expr), (#expr), __FILE__, __FUNCTION__, __LINE__, (hint))



template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}


#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void TestAddDocuments() {
    const int doc_id = 37;
    const string content = "my name is artem and what is yours"s;
    const vector<int> ratings = {4, 5, 6};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(((server.FindTopDocuments("my name artem"s))[0].id == 37), "Adding new document"s);
    }
}


// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Doc isn't found"s);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT((server.FindTopDocuments("in"s).empty()), "problems with using stop-words"s);
    }
}

//проверка учёта минус-слов
void TestEliminateMinusWordsFromAddedDocumentContent() {
    const int doc_id = 54;
    const string content = "i love practicum and c++ pain"s;
    const vector<int> ratings = {5, 5, 5};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT((server.FindTopDocuments("-pain python"s).empty()),"problems with using minus-words"s);

    }

}


//проверка правильности сортировки
void TestSortingRel() {
    const int doc_id1 = 12;
    const string content1 = "hi my name is tikatika slim shady";
    const vector<int> ratings1 = {2, 3, 4};

    const int doc_id2 = 90;
    const string content2 = "Slim shady has become eminem after his most popular album"s;
    const vector<int> ratings2 = {2, 3, 4};

    const int doc_id3 = 45;
    const string content3 = "All i see is you words"s;
    const vector<int> ratings3 = {2, 3, 4};
    {
    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    //проверяем, что было отобрано только 2 документа из 3-х
    auto result = server.FindTopDocuments("Eminem slim shady"s);
    ASSERT(result.size() == 2);
    //проверяем, что они находятся в нужной последовательности
    bool isright = true;
    for (size_t i = 0; i < result.size() - 1; i++) {
        if (result[i].relevance < result[i+1].relevance) {
            isright = false;

        }
    }
    ASSERT_HINT(isright, "sorting wrong"s);
    
    }
}

//Проверка правильности подсчёта рейтинга
void TestCountingRating() {
    SearchServer server;
    const int doc_id1 = 12;
    const int doc_id2 = 23;
    const int doc_id3 = 45;

    const string content1 = "hi my name is tikatika slim shady"s;
    const string content2 = "I've created a monster, cause nobody wants to see Marshall no more"s;
    const string content3 = "And if you ask me to, daddy's gonna buy you a mockingbird, I'ma give you the world"s;

    const vector<int> ratings1 = {2, 3, 4};
    const vector<int> ratings2 = {-2, -3, -4};
    const vector<int> ratings3 = {2, -2, 6};
    {
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    ASSERT_HINT(((server.FindTopDocuments("tikatika slim shady"s))[0].rating == 3), "Positive ratings"s);

    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    ASSERT_HINT(((server.FindTopDocuments("Marshall no more"s))[0].rating == -3), "Negative ratings"s);

    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    ASSERT_HINT(((server.FindTopDocuments("buy you a mockingbird"s))[0].rating == 2), "Mixed ratings"s);
    }



}

//проверка поиска по определенному статусу
void TestStatus() {
    const int doc_id1 = 12;
    const string content1 = "hi my name is tikatika slim shady";
    const vector<int> ratings1 = {2, 3, 4};

    const int doc_id2 = 90;
    const string content2 = "Slim shady has become eminem after his most popular album"s;
    const vector<int> ratings2 = {2, 3, 4};

    const int doc_id3 = 45;
    const string content3 = "Venom, (I got that) adrenaline momentum, And I'm not knowin' when I'm"s;
    const vector<int> ratings3 = {2, 3, 4};

    const int doc_id4 = 69;
    const string content4 = "You better lose yourself in the music, the moment, you own it, you better never let it go"s;
    const vector<int> ratings4 = {2, 3, 4};    
    {
        SearchServer server;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::BANNED, ratings2);
        server.AddDocument(doc_id3, content3, DocumentStatus::REMOVED, ratings3);
        server.AddDocument(doc_id4, content4, DocumentStatus::IRRELEVANT, ratings4);


        ASSERT_HINT(((server.FindTopDocuments("hi my name"s, DocumentStatus::BANNED)).empty()), "Status - banned"s);
        ASSERT_HINT((((server.FindTopDocuments("hi my name"s))[0].id == 12)), "Status - actual"s);
        ASSERT_HINT((((server.FindTopDocuments("I'm venom"s, DocumentStatus::REMOVED))[0].id == 45)), "Status - removed"s);
        ASSERT_HINT(((server.FindTopDocuments("lose yourself in the music"s, DocumentStatus::IRRELEVANT))[0].id == 69), "Status - irrelevant"s);
    }
}

void TestRel() { /// Проверка правильного вычисления релевантности 
    const int doc_id1 = 12;
    const string content1 = "hi my name is tikatika slim shady";
    const vector<int> ratings1 = {2, 3, 4};

    const int doc_id2 = 90;
    const string content2 = "slim shady has become eminem after his most popular album"s;
    const vector<int> ratings2 = {2, 3, 4};

    const int doc_id3 = 45;
    const string content3 = "Venom, I got that adrenaline momentum, And I'm not knowin' when I'm"s;
    const vector<int> ratings3 = {2, 3, 4};

{
    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);

    // правильные значения релевантности посчитаны вручную
    ASSERT_HINT((std::abs((server.FindTopDocuments("tikatika"))[0].relevance - 0.15694461) < diff), "One word in one doc"s); 
    ASSERT_HINT((std::abs((server.FindTopDocuments("adrenaline momentum"))[0].relevance - 0.09155102) < diff), "Two words in one doc"s); 
    ASSERT_HINT((std::abs((server.FindTopDocuments("slim shady"))[0].relevance - 0.11584717) < diff), "Two words in two docs"s); 

        
}
}

void TestPredicat() {
    const int doc_id1 = 12;
    const string content1 = "hi my name is tikatika slim shady";
    const vector<int> ratings1 = {10, 10, 10};

    const int doc_id2 = 90;
    const string content2 = "slim shady has become eminem after his most popular album"s;
    const vector<int> ratings2 = {2, 3, 4};

    const int doc_id3 = 45;
    const string content3 = "venom (I got that) adrenaline momentum, And I'm not knowin' when I'm"s;
    const vector<int> ratings3 = {2, 3, 4};

    const int doc_id4 = 69;
    const string content4 = "You better lose yourself in the music, the moment, you own it, you better never let it go"s;
    const vector<int> ratings4 = {2, 3, 4};    
{
    SearchServer server;
    server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::ACTUAL, ratings3);
    server.AddDocument(doc_id4, content4, DocumentStatus::IRRELEVANT, ratings4);

    ASSERT_HINT((server.FindTopDocuments("lose yourself"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::IRRELEVANT; })[0].id == 69), "One doc for status"s);
    ASSERT_HINT((server.FindTopDocuments("slim shady"s, [](int document_id, DocumentStatus status, int rating) { return rating == 10; })[0].id == 12), "Using ratings"s);
    ASSERT_HINT((server.FindTopDocuments("venom"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })[0].id == 45), "sorting after selection by predicate"s);
    ASSERT_HINT((server.FindTopDocuments("slim shady"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::REMOVED; }).empty()),"non-existent status"s);
}
}


// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocuments);
    RUN_TEST(TestEliminateMinusWordsFromAddedDocumentContent);
    RUN_TEST(TestCountingRating);
    RUN_TEST(TestSortingRel);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRel);
    RUN_TEST(TestPredicat);
}

// --------- Окончание модульных тестов поисковой системы -----------// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}