// http://coliru.stacked-crooked.com/a/4675a74c8124750f

// Your name: Seth Heeren
// Your email: bugs@sehe.nl

// - Aggressively minimizes allocations (constructs only 1 string unless an exception is thrown)
// - Uses destructuring, if-with-initializer, make_ostream_joiner, string_view, literals, custom range
//   constructor deduction guides (std::insert_iterator), inline variables
// - Many c++14/z
// - For troll value only: `using namespace std`
// - It's a shame the specs missed the grand opportunity to exercise
//   std::quoted! That's a game changer for things like this
// - The required "edge case" error reporting is iffy, so I just did the
//   simplest thing that works according to specs. Fire the customer!

#include <algorithm>
#include <experimental/iterator>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <tuple>
#include <vector>

using namespace std;
using namespace string_literals;
using experimental::make_ostream_joiner;

inline auto index_of = [](auto const& c, auto const& v) { return find(begin(c), end(c), v) - begin(c); };

struct csv_io {
    csv_io(string_view chars) : _delims(chars) {}

    auto& operator[](size_t i) const { return _cols.at(i); }
    auto& operator[](size_t i)       { return _cols.at(i); }

    struct error : runtime_error { error(string msg) : runtime_error(msg) {} };

  private:
    string              _readbuf;
    vector<string_view> _cols;
    string_view         _delims;

    friend auto begin(csv_io& d) { return d._cols.begin(); }
    friend auto end  (csv_io& d) { return d._cols.end(); }
    friend auto begin(csv_io const& d) { return d._cols.begin(); }
    friend auto end  (csv_io const& d) { return d._cols.end(); }

    friend ostream& operator<<(ostream& os, csv_io const& o) {
        copy(begin(o), end(o), make_ostream_joiner(os, ","));
        return os << "\n";
    }

    auto take_head(string_view sv) {
        if (auto pos = sv.find_first_of(_delims); pos != string_view::npos)
            return tuple {sv.substr(0, pos), sv.substr(pos+1)};
        else
            return tuple {sv, string_view{}};
    }

    friend istream& operator>>(istream& is, csv_io& o) {
        o._cols.clear();

        if (getline(is, o._readbuf)) {
            auto out          = insert_iterator(o._cols, end(o._cols));
            auto [head, tail] = o.take_head(o._readbuf);

            while (head.size() || tail.size()) {
                *out++ = head;
                tie(head,tail) = o.take_head(tail);
            }
        } else {
            if (!is.eof()) throw error("input file missing");
        }

        return is;
    }

};

int main(int argc, char** argv) try {
    auto shift = [=]() mutable { return *++argv; }; // shell/perl - inspired

    if (argc != 5) {
        cerr << "Usage: " << *argv << " <infile> <column> <replacement> <outfile>\n";
        return 255;
    }

    auto [infile, column, replacement, outfile] = array<string_view, 4> {shift(), shift(), shift(), shift()};

    ifstream ifs(infile.data());
    ofstream ofs(outfile.data());
    csv_io csv(",");
    if (ifs >> csv) try {
        auto const N = index_of(csv, column);
        ofs << csv;

        while (ifs >> csv) {
            csv[N] = replacement;
            ofs << csv;
        }
    } catch(out_of_range const& e) {
        cerr << "column name doesn't exist in the input file\n";
    }
} catch(csv_io::error const& e) {
    cerr << e.what() << '\n';
}

