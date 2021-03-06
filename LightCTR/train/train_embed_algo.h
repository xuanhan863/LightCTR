//
//  train_embed_algo.h
//  LightCTR
//
//  Created by SongKuangshi on 2017/10/17.
//  Copyright © 2017年 SongKuangshi. All rights reserved.
//

#ifndef train_embed_algo_h
#define train_embed_algo_h

#include <stdio.h>
#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include "../common/thread_pool.h"
#include "../util/activations.h"
#include "../util/random.h"
#include "assert.h"
using namespace std;

#define IGNORE_CHAR ch != ' ' && ch != '\n' && ch != '\t' && ch != '.'
#define BLETTER (ch>='a' && ch<='z')||(ch>='A' && ch<='Z')

class Train_Embed_Algo {
    struct Node {
        int left, right;
        double *weight; // LR weight
        double frequency;
        Node() {
            left = -1;
            right = -1;
            frequency = 0.0f;
        }
    };
    struct cmp {
        bool operator()(pair<int, int> &a, pair<int, int> &b){
            if(a.first == b.first)
                return a.second > b.second;
            return a.first > b.first;
        }
    };
public:
    Train_Embed_Algo(string vocabFile, string _textFile, size_t _epoch,
                     size_t _window_size, size_t _emb_dimention, size_t _vocab_cnt):
    textFile(_textFile), epoch(_epoch), window_size(_window_size),
    emb_dimention(_emb_dimention), vocab_cnt(_vocab_cnt) {
        threadpool = new ThreadPool(thread::hardware_concurrency());
        loadVocabFile(vocabFile); // import vocab word_id and frequency
        loadTextFile(&textStream); // import documents joined by word_id
        init();
        InitNegSampleTable();
    }
    ~Train_Embed_Algo() {
        delete [] treeArry;
        delete [] negSampleTable;
        delete [] negWeight;
        delete [] word_embedding;
    }
    
    void Train();
    void EmbeddingCluster(shared_ptr<vector<int> >, size_t);
    
private:
    
    void init();
    int NextWord(ifstream* fin, char *word) {
        char ch = '\n';
        int len = 0;
        while(!fin->eof() && len == 0){
            while(fin->get(ch)) { // clean front no-letter
                if (ch == '\n') {
                    return -1; // new document
                }
                if (BLETTER) {
                    break;
                }
            }
            word[len++] = ch;
            while(fin->get(ch) && BLETTER) {
                word[len++] = tolower(ch);
            }
        }
        word[len] = '\0';
        return len; // when len==0 means EOF
    }
    
    void TrainDocument(size_t, size_t);
    void saveModel();

    void loadVocabFile(string vocabFile) {
        ifstream fin_;
        string line;
        char str[128];
        int wid = -1, fre, prefre = 0;
        fin_.open(vocabFile, ios::in);
        if(!fin_.is_open()){
            cout << "open file error!" << endl;
            exit(1);
        }
        
        word_embedding = new vector<double>*[vocab_cnt];
        vocabTable.reserve(vocab_cnt);
        vocabString.reserve(vocab_cnt);
        while(!fin_.eof()){
            getline(fin_, line);
            const char *pline = line.c_str();
            if(sscanf(pline, "%d %s %d", &wid, str, &fre) >= 1){
                assert(!isnan(wid) && (wid == 0 ^ (wid != 0 && fre <= prefre) == 1));
                prefre = fre;
                word_frequency.push_back(fre);
                vocabTable[string(str)] = wid;
                vocabString.emplace_back(string(str));
                threadpool->addTask([&, wid]() {
                    word_embedding[wid] = new vector<double>();
                    word_embedding[wid]->reserve(emb_dimention);
                    for (int i = 0; i < emb_dimention; i++) { // random init embedding
                        double r = UniformNumRand() - 0.5f;
                        assert(word_embedding[wid]);
                        word_embedding[wid]->push_back(r / emb_dimention);
                    }
                });
            }
            if (vocabTable.size() >= vocab_cnt) {
                break;
            }
        }
        threadpool->join();
        assert(word_frequency.size() == vocab_cnt);
    }
    void loadTextFile(ifstream* _textStream) {
        _textStream->open(textFile, ios::in);
        if(!_textStream->is_open()){
            cout << "open text file error!" << endl;
            exit(1);
        }
    }
    
    void InitNegSampleTable() {
        negWeight = new vector<double>[vocab_cnt];
        for (size_t v = 0; v < vocab_cnt; v++) {
            negWeight[v].resize(emb_dimention);
            // negative sampling weight init with 0
            fill(negWeight[v].begin(), negWeight[v].end(), 0);
        }
        
        int a;
        long long sum_word_pow = 0; // normalizer
        double d1, power = 0.75;
        negSampleTable = new int[negTable_size];
        for (a = 0; a < vocab_cnt; a++) {
            sum_word_pow += pow(word_frequency[a], power);
        }
        int wid = 0;
        d1 = pow(word_frequency[wid], power) / (double)sum_word_pow;
        for (a = 0; a < negTable_size; a++) {
            negSampleTable[a] = wid;
            if (a / (double)negTable_size > d1) {
                wid++;
                d1 += pow(word_frequency[wid], power) / (double)sum_word_pow;
            }
            if (wid >= vocab_cnt) {
                wid = (int)vocab_cnt - 1;
            }
        }
    }
    
    size_t vocab_cnt, emb_dimention, window_size, negSample_cnt;
    
    const int negTable_size = 1e8;
    int* negSampleTable;
    vector<double>* negWeight;
    
    unordered_map<string, size_t> vocabTable; // hash vocab to id
    vector<string> vocabString; // id to string
    vector<double>* *word_embedding; // frequent order id to embedding
    vector<int> word_frequency;
    
    string textFile;
    ifstream textStream;
    
    double learning_rate;
    
    Node* treeArry; // Huffman tree
    Node* treeRoot;
    vector<string> word_code; // huffman code of word id
    
    Sigmoid sigmoid;
    
    size_t epoch;
    ThreadPool *threadpool;
    mutex lock;
};

#endif /* train_embed_algo_h */
