#include "../utils/helper.h"
#include <queue>
using namespace std;

bool PROJECT = false;

const int D = 30;
const int K = 3;
const long long max_size = 2000;         // max length of strings
const long long N = 40;                  // number of closest words that will be shown
const long long max_w = 50;              // max length of vocabulary entries

unordered_map<string, vector<double>> word2vec;

void loadVector(string filename)
{
    FILE *f;
    char st1[max_size];
    char file_name[max_size], st[100][max_size];
    float dist, len, bestd[N], vec[max_size];
    long long words, size, a, b, c, d, cn, bi[100];
    char ch;
    float *M;
    char *vocab;
    f = tryOpen(filename, "rb");
    if (f == NULL) {
        printf("Input file not found\n");
        return;
    }
    fscanf(f, "%lld", &words);
    fscanf(f, "%lld", &size);
    vocab = (char *)malloc((long long)words * max_w * sizeof(char));
    M = (float *)malloc((long long)words * (long long)size * sizeof(float));
    if (M == NULL) {
        printf("Cannot allocate memory: %lld MB    %lld  %lld\n", (long long)words * size * sizeof(float) / 1048576, words, size);
        return;
    }
    
    vector< vector<double> > axis(D, vector<double>(size, 0));
    for (int i = 0; i < D; ++ i) {
        for (int j = 0; j < size; ++ j) {
            axis[i][j] = sample_normal();
        }
    }
    
    for (b = 0; b < words; b++) {
        a = 0;
        while (1) {
            vocab[b * max_w + a] = fgetc(f);
            if (feof(f) || (vocab[b * max_w + a] == ' ')) break;
            if ((a < max_w) && (vocab[b * max_w + a] != '\n')) a++;
        }
        vocab[b * max_w + a] = 0;
        for (a = 0; a < size; a++) fread(&M[a + b * size], sizeof(float), 1, f);
        /*
        len = 0;
        for (a = 0; a < size; a++) len += M[a + b * size] * M[a + b * size];
        len = sqrt(len);
        for (a = 0; a < size; a++) M[a + b * size] /= len;
        */
        string word = &vocab[b * max_w];
        double norm = 0;
        vector<double> vec;
        if (PROJECT) {
            vec.resize(D, 0);
            for (int d = 0; d < D; ++ d) {
                double dot = 0;
                for (a = 0; a < size; a ++) {
                    dot += M[a + b * size] * axis[d][a];
                }
                vec[d] = dot;
                norm += dot * dot;
            }
        } else {
            vec.resize(size, 0);
            for (a = 0; a < size; a ++) {
                vec[a] = M[a + b * size];
                norm += vec[a] * vec[a];
            }
        }
        norm = sqrt(norm);
        for (size_t d = 0; d < vec.size(); ++ d) {
            vec[d] /= norm;
        }
        word2vec[word] = vec;
    }
    fclose(f);
}

unordered_map<string, double> unigrams, phrases;

void loadPatterns(string folder)
{
    const int maxLen = 6;
    for (int length = 1; length <= maxLen; ++ length) {
        ostringstream filename;
        filename << "length" << length << ".csv";
        
        FILE* in = tryOpen(folder + "/" + filename.str(), "r");
        if (in == NULL) {
            continue;
        }
        while (getLine(in)) {
            vector<string> tokens = splitBy(line, ',');
            string phrase = tokens[0];
            double prob;
            fromString(tokens[3], prob);
            
            if (length == 1) {
                unigrams[phrase] = 0;//prob;
            } else {
                for (size_t i = 0; i < phrase.size(); ++ i) {
                    if (phrase[i] == ' ') {
                        phrase[i] = '_';
                    }
                }
                phrases[phrase] = prob;
            }
        }
        fclose(in);
    }
}

int main(int argc, char *argv[])
{
    double alpha = 0;
    if (argc != 6 || sscanf(argv[5], "%lf", &alpha) != 1 || alpha < 0 || alpha > 1) {
        printf("[usage] <vector.bin> <length*.csv folder path> <output: unigram-rank> <0/1 project or not> <alpha: ratio for keep the previous value>\n");
        return 0;
    }
    
    if (strcmp(argv[4], "1")) {
        PROJECT = false;
    } else {
        PROJECT = true;
    }
    
    loadVector(argv[1]);
    loadPatterns(argv[2]);
    
    cerr << unigrams.size() << endl;
    cerr << phrases.size() << endl;

    vector<string> unigramList;
    FOR (unigram, unigrams) {
        unigramList.push_back(unigram->first);
    }
    
    vector<string> wordList;
    unordered_map<string, double> word;
    FOR (unigram, unigrams) {
        word[unigram->first] = unigram->second;
        if (word2vec.count(unigram->first)) {
            wordList.push_back(unigram->first);
        }
    }
    FOR (phrase, phrases) {
        word[phrase->first] = phrase->second;
        if (word2vec.count(phrase->first)) {
            wordList.push_back(phrase->first);
        }
    }
    
    unordered_map<string, vector<pair<string, double>>> neighbors;
    vector<vector<pair<string, double>>> nns(wordList.size(), vector<pair<string, double>>(K, make_pair("", 0)));
    
    bool buffered = false;
    if (true) {
        FILE* in = tryOpen("results/neighbors.buff.txt", "r");
        if (in != NULL) {
            cerr << "neighbor from buffer" << endl;
            buffered = true;
            while (getLine(in)) {
                vector<string> tokens = splitBy(line, '\t');
                string w = tokens[0];
                for (int i = 1; i + 1 < tokens.size(); i += 2) {
                    string neighbor = tokens[i];
                    myAssert(word.count(neighbor), "wrong neighbor!! " + neighbor + "\n" + line);
                    double similarity;
                    fromString(tokens[i + 1], similarity);
                    neighbors[w].push_back(make_pair(neighbor, similarity));
                }
            }
            fclose(in);
            cerr << neighbors.size() << " loaded, " << wordList.size() << " words in total" << endl;
            
            for (size_t i = 0; i < wordList.size(); ++ i) {
                nns[i] = neighbors[wordList[i]];
            }
        }
    }

    #pragma omp parallel for schedule(dynamic, 1000)
    for (int i = 0; i < unigramList.size(); ++ i) {
        const string &key = unigramList[i];
        if (!word2vec.count(key)) {
            unigrams[key] = 0;
            continue;
        }
        priority_queue < pair<double, string> > heap;
        const vector<double> &v = word2vec[key];
        double maxi = 0;
        FOR (phrase, phrases) {
            const string &p = phrase->first;
            if (word2vec.count(p)) {
                const vector<double> &vp = word2vec[p];
                double dot = 0;
                double sum1 = 1, sum2 = 1;
                for (size_t i = 0; i < vp.size(); ++ i) {
                    dot += vp[i] * v[i];
                    sum1 -= vp[i] * vp[i];
                    sum2 -= v[i] * v[i];
                    if (heap.size() == K && -heap.top().first >= dot + sqrt(sum1 * sum2)) {
                        break;
                    }
                }
                maxi = max(maxi, dot);
                if (heap.size() < K || dot > -heap.top().first) {
                    heap.push(make_pair(-dot, phrase->first));
                    if (heap.size() > K) {
                        heap.pop();
                    }
                }
            }
        }
        double sum = 0;
        double sum_weight = 0;
        while (heap.size() > 0) {
            double similarity = -heap.top().first;
            string phrase = heap.top().second;
            double score = phrases[phrase];
            similarity /= maxi;

            sum_weight += similarity;
            sum += similarity * score;
            
            heap.pop();
        }
        sum_weight = 3;
        unigrams[key] = sum / sum_weight;
    }
    cerr << "unigram initialized" << endl;
    
    for (int iter = 0; iter < 10; ++ iter) {
        cerr << "iter " << iter << endl;
        vector<double> newScores(wordList.size(), 0);
        #pragma omp parallel for schedule(dynamic, 1000)
        for (size_t i = 0; i < wordList.size(); ++ i) {
            const string &wi = wordList[i];
            if (iter == 0 && neighbors.size() == 0) {
                const vector<double> &v = word2vec[wi];
                priority_queue<pair<double, string>> heap;
                double maxi = 0;
                for (size_t j = 0; j < wordList.size(); ++ j) {
                    if (i != j) {
                        const string &wj = wordList[j];
                        const vector<double> &vp = word2vec[wj];
                        double dot = 0;
                        double sum1 = 1, sum2 = 1;
                        for (size_t d = 0; d < vp.size(); ++ d) {
                            dot += vp[d] * v[d];
                            sum1 -= vp[d] * vp[d];
                            sum2 -= v[d] * v[d];
                            if (heap.size() == K && -heap.top().first >= dot + sqrt(sum1 * sum2)) {
                                break;
                            }
                        }
                        maxi = max(maxi, dot);
                        if (heap.size() < K || dot > -heap.top().first) {
                            heap.push(make_pair(-dot, wj));
                            if (heap.size() > K) {
                                heap.pop();
                            }
                        }
                    }
                }
                
                myAssert(heap.size() == K, "too less neighbors");
                
                int ptr = 0;
                while (heap.size() > 0) {
                    double similarity = -heap.top().first;
                    string word = heap.top().second;
                    heap.pop();
                    similarity /= maxi;
                    nns[i][ptr ++] = make_pair(word, similarity);
                }
            }
            
            double sum = 0, sum_weight = 0;
            FOR (neighbor, nns[i]) {
                const string &wj = neighbor->first;
                const double &similarity = neighbor->second;
                double score = word[wj];
                sum_weight += similarity;
                sum += similarity * score;
            }
            newScores[i] = sum / sum_weight;
        }
        for (size_t i = 0; i < wordList.size(); ++ i) {
            word[wordList[i]] = word[wordList[i]] * alpha + newScores[i] * (1 - alpha);
        }
        if (iter == 0) {
            for (size_t i = 0; i < wordList.size(); ++ i) {
                neighbors[wordList[i]] = nns[i];
            }
        }
    }
    
    if (!buffered) {
        FILE* out = tryOpen("results/neighbors.buff.txt", "w");
        FOR (iter, neighbors) {
            fprintf(out, "%s", iter->first.c_str());
            FOR (pair, iter->second) {
                fprintf(out, "\t%s\t%.10f", pair->first.c_str(), pair->second);
            }
            fprintf(out, "\n");
        }
        fclose(out);
    }

    vector<pair<double, string>> order;
    FOR (w, word) {
        order.push_back(make_pair(w->second, w->first));
    }
    sort(order.rbegin(), order.rend());
    
    FILE* out = tryOpen(argv[3], "w");
    FOR (unigram, order) {
        fprintf(out, "%s,%.10f\n", unigram->second.c_str(), unigram->first);
    }
    fclose(out);
}
