# Parallel-Word-Mapper-and-Reducer

Note: (*) -> e vorba despre bariera utilizata.

    Din fisierul de intrare preiau numarul de fisiere cu care urmeaza sa lucrez,
precum si numele acestora. Pentru a le stoca, am creat un vector de structuri
'MyFile', fiecare continand variabile de nume, id, size ale unui fisier.
Pentru a obtine dimensiunea (size), am creat o functie 'get_file_size' ce muta
cursorul la finalul fisierului si returneaza pozitia la care se afla. De asemenea,
id-ul fisierului porneste de la 1, nu de la 0. In continuare, sortez vectorul creat,
descrescator, in functie de dimensiunea fiecarui fisier pentru o distribuire a 
thread-urilor Mapper asupra fisierelor mai echilibrata.
Conform cerintei, creez (nr_mappers + nr_reducers) thread-uri in vectorul 'threads'.

-Operatiunea de Map:
    nr_mappers thread-uri Map lucreaza asupra functiei 'mapper_args'. Pentru parsarea
argumenteleor necesare functiei, am creat o structura MapperArgs. Am trecut vectorul
de structuri 'MyFile' la o coada de astfel de structuri pentru eliminarea usoara a
fisierelor din ea prin operatia pop() (O(1)).
    In cadrul functiei creez un loop in care intra thread-urile create. Fiecare mapper
preia un fisier din coada de fisiere, dupa care il elimina din coada, aceste operatii
critice fiind sincronizate cu un mutex. In continuare, se deschide fisierul si se
extrage fiecare cuvant din el, dupa care sunt eliminate caracterele non-alfabetice si se
trec toate majusculele la litere mici. Ulterior, intr-un map 'word_map', pentru fiecare
cuvant transformat este salvat indexul fisierului in care a fost gasit. Daca a fost gasit
de mai multe ori in cadrul unui fisier, nu conteaza, intrucat indecsii sunt salvati
intr-un 'set', ce nu accepta duplicate. Mai apoi, se inchide fisierul de intrare.
    (*)In final, folosesc o bariera comuna menita sa astepte atat thread-urile mapper cat
si reducer. Momentan, in afara loop-ului vor fi asteptate thread-urile mapper sa se
finalizeze inainte de a continua programul.
    De asemenea, in main, map-urile in care lucreaza thread-urile mapper sunt oferite ca
argumente si fac parte dintr-un vector de map-uri, unindu-se astfel intr-o singura 
structura.

-Operatiunea de Reduce:
    nr_reducers thread-uri Reducer lucreaza asupra functiei 'reducer_args'. Pentru parsarea
argumenteleor necesare functiei, am creat o structura ReducerArgs. Am creat o coada ce
contine toate literele alfabetului, pentru ca un thread Reducer sa lucreze la un moment de
timp cu toate cuvintele care incep cu acea litera.
    (*)In cadrul functiei incep cu apelarea barierei comune. Aceasta permite continuarea
programului doar in cazul in care au terminat atat thread-urile mapper (asteptate la final
de 'mapper_func'), dar au fost si create toate thread-urile reducer si pot incepe executia.
    In continuare, creez un loop in care intra thread-urile create. Fiecare Reducer preia
o litera din coada ce contine toate literele alfabetului, dupa care o elimina din coada,
aceste operatii critice fiind sincronizate cu un mutex. Pentru agregarea listelor partiale,
creez un map mare 'reduced_map'. Pentru fiecare map din vectorul 'word_maps', se cauta 
cuvintele care incep cu litera curenta, si se trec in 'reduced_map' alaturi de indecsii
asociati.
    Mai apoi, acest map este convertit intr-un vector de perechi pentru a fi sortat in
functie de numarul de indecsi asociati fiecarui cuvant (in cate fisiere apare). In caz de
egalitate, sortarea se face alfabetic.
    In final, se creeaza un fisier de output "litera_curenta.txt" in care sunt printate toate
perechile vectorului sortat, conform cerintei: "cuvant:[1 2 3]".

    La sfarsitul programului, sunt asteptate cele nr_mappers + nr_reducers thread-uri intr-un
singur for.
