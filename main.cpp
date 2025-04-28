#include <iostream>
#include <fstream>
#include <string>
#include <pthread.h>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <queue>
#include <algorithm>

using namespace std;

struct MyFile {
    string name;
    int file_id;
    long size; // dimensiune fisier (indexul sfarsitului fisierului)
};

struct MapperArgs{
    map<string, set<int>> *word_map; //map de cuvinte cu indecsii fisierelor pt un thread mapper
    queue<MyFile> *all_files; //coada de fisiere
    mutex *mapper_mtx;
    pthread_barrier_t *barrier_threads;
};

struct ReducerArgs{
    vector<map<string, set<int>>> *word_maps; // vector cu map-urile rezultate de mapperi
    queue<char> *letters; // coada cu literele alfabetului
    mutex *reducer_mtx;
    pthread_barrier_t *barrier_threads;
};

// gaseste dimensiunea unui fisier:
long get_file_size(const string &file_name)
{
    ifstream file(file_name);
    if (!file.is_open()) {
        perror("get_file_size");
        return -1;
    }

    file.seekg(0, ios::end); // muta cursorul la finalul fisierului
    long size = file.tellg(); // ofera pozitia cursorului (finalul fisierului)

    file.close();

    return size;
}

// compare pt sortarea fisierelor dupa dimensiuni, descrescator:
bool cmp_files(const MyFile &a, const MyFile &b) {
    return a.size > b.size;
}

// compare pt sortarea cuvintelor dupa nr de fisiere in care apar, descrescator:
// (in caz de egalitate, se sorteaza alfabetic)
bool cmp_words(const pair<string, set<int>> &a, const pair<string, set<int>> &b) {
    if (a.second.size() == b.second.size()) {
        return a.first < b.first;
    } else {
        return a.second.size() > b.second.size();
    }
}

// operatia unui mapper de gasire a cuvintelor din fiecare fisier cu care lucreaza:
void *mapper_func(void *args) {
    MapperArgs *mapper_args = (MapperArgs *)args;
    map<string, set<int>> &word_map = *(mapper_args->word_map);
    queue<MyFile> &all_files = *(mapper_args->all_files);
    mutex &mapper_mtx = *(mapper_args->mapper_mtx);
    pthread_barrier_t &barrier_threads = *(mapper_args->barrier_threads);

    while (true) {
        MyFile file;

        // sectiune critica - extragere fisier curent din coada:
        mapper_mtx.lock();
        if (!all_files.empty()) {
            file = all_files.front();
            all_files.pop();
        } else {
            mapper_mtx.unlock();
            break; // nu mai sunt fisiere de procesat
        }
        mapper_mtx.unlock();

        ifstream file_stream(file.name);
        if (!file_stream.is_open()) {
            perror("mapper_input");
            continue;
        }

        // extrage fiecare cuvant din fisierul curent:
        string word;
        while (file_stream >> word) {
            word.erase(remove_if(word.begin(), word.end(), [](char c) {
                return !isalpha(c); // elimina caracterele non-alfabetice
            }), word.end());

            // trecerea cuvintelor la litere mici:
            transform(word.begin(), word.end(), word.begin(), ::tolower);

            // adaugarea cuvant-fisierul in care a fost gasit,
            // in map fara duplicate (set nu accepta duplicate):
            word_map[word].insert(file.file_id);
        }

        file_stream.close();
    }

    // asteptarea threadurilor mapper:
    pthread_barrier_wait(&barrier_threads);

    return NULL;
}

// operatiile unui reducer pentru ficare litera cu care lucreaza:
// - agregare liste, sortare cuvinte si output -
void *reducer_func(void *args) {
    ReducerArgs *reducer_args = (ReducerArgs *)args;
    vector<map<string, set<int>>> &word_maps = *(reducer_args->word_maps);
    queue<char> &letters = *(reducer_args->letters);
    mutex &reducer_mtx = *(reducer_args->reducer_mtx);
    pthread_barrier_t &barrier_threads = *(reducer_args->barrier_threads);

    // asteptarea threadurilor mapper si reducer inainte de a incepe:
    // (bariera poate fi plina doar dupa completarea tuturor threadurilor mapper)
    pthread_barrier_wait(&barrier_threads);

    while (true) {
        char letter;

        // sectiune critica - extragere litera curenta din coada:
        reducer_mtx.lock();
        if (!letters.empty()) {
            letter = letters.front();
            letters.pop();
        } else {
            reducer_mtx.unlock();
            break;
        }
        reducer_mtx.unlock();

        // liste partiale in agregate,
        // doar pentru cuvintele care incep cu litera curenta:
        map<string, set<int>> reduced_map;
        for (const auto &word_map : word_maps) {
            for (const auto &word : word_map) {
                if(word.first[0] == letter) {
                    reduced_map[word.first].insert(word.second.begin(), word.second.end());
                }
            }
        }

        // sortare descrescatoare dupa nr de fisiere in care apare cuvantul:
        vector<pair<string, set<int>>> sorted_words(reduced_map.begin(), reduced_map.end());
        sort(sorted_words.begin(), sorted_words.end(), cmp_words);

        // fisierul de output pentru litera curenta:
        ofstream out_file(string(1, letter) + ".txt");
        if (!out_file.is_open()) {
            perror("reducer_output");
            continue;
        }

        // scrierea in output conform cerintei: and:[1 2 3]
        for (const auto &word : sorted_words) {
            out_file << word.first << ":[";
            out_file << *word.second.begin();

            for (auto index = ++word.second.begin(); index != word.second.end(); index++) {
                out_file << " " << *index;
            }

            out_file << "]" << endl;
        }

        out_file.close();
    }

    return NULL;
}
    


int main(int argc, char **argv)
{
    // ---------------------CITIRI ARGUMENTE + FISIERE---------------------
    int nr_mappers = atoi(argv[1]);
    int nr_reducers = atoi(argv[2]);

    // fisier intrare:
    ifstream entry_file(argv[3]);
    if (!entry_file)
    {
        perror("fisier_intrare");
        return 1;
    }

    // citirea fisierului de intrare:
    int nr_files = 0; // nr de fisiere
    entry_file >> nr_files;

    // vector cu fisierele date in fisierul de intrare:
    vector<MyFile> all_files;

    for (int i = 0; i < nr_files; i++)
    {
        string file_name = "";
        entry_file >> file_name;

        long file_size = get_file_size(file_name);

        // adaugarea fisierului in vector cu indexul mai mare cu 1:
        // (fisier 0 -> fisier 1):
        all_files.push_back({file_name, i + 1, file_size});
    }

    // inchiderea fisierului de intrare:
    entry_file.close();

    // sortare fisiere descrescator dupa dimensiunea lor:
    sort(all_files.begin(), all_files.end(), cmp_files);

    // ---------------------CREARE THREAD-URI + BARIERA---------------------

    // crearea thread-urilor:
    vector<pthread_t> threads(nr_mappers + nr_reducers);
    // bariera comuna pentru astepatarea thread-urilor mapper si reducer:
    // (inainte de a incepe reducerii, mapperii trebuie sa termine)
    pthread_barrier_t barrier_threads;
    pthread_barrier_init(&barrier_threads, NULL, nr_mappers + nr_reducers);

    // --------------------THREAD-URI MAPPER--------------------

    // trecerea intr-o coada a fisierelor date:
    queue<MyFile> all_files_queue;
    for (const auto &file : all_files) {
        all_files_queue.push(file);
    }

    // vector cu rezultatele threadurilor mapper combinate
    vector<map<string, set<int>>> word_maps(nr_mappers);

    // argumentele thread-urilor mapper:
    vector<MapperArgs> mapper_args(nr_mappers);

    mutex mapper_mtx; // mutexul mapperilor

    // thread-urile mapper:
    for (int i = 0; i < nr_mappers; i++) {
        mapper_args[i] = {&word_maps[i], &all_files_queue, &mapper_mtx, &barrier_threads};
        pthread_create(&threads[i], NULL, mapper_func, &mapper_args[i]);
    }

    // --------------------THREAD-URI REDUCER--------------------

    // crearea cozii cu literele alfabetului:
    queue<char> letters;
    for (char c = 'a'; c <= 'z'; c++) {
        letters.push(c);
    }

    // argumentele thread-urilor reducer:
    vector<ReducerArgs> reducer_args(nr_reducers);

    mutex reducer_mtx; // mutexul reducerilor

    // thread-urile reducer:
    for (int i = 0; i < nr_reducers; i++) {
        reducer_args[i] = {&word_maps, &letters, &reducer_mtx, &barrier_threads};
        pthread_create(&threads[i], NULL, reducer_func, &reducer_args[i]);
    }

    // --------------------ASTEPTARE THREAD-URI-----------------

    for (int i = 0; i < nr_mappers + nr_reducers; i++)
    {
        pthread_join(threads[i], NULL);
    }

    return 0;
}