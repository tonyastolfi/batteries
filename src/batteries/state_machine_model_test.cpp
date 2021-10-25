// Copyright 2021 Anthony Paul Astolfi
//
#include <batteries/state_machine_model.hpp>
//
#include <batteries/state_machine_model.hpp>

#include <boost/functional/hash.hpp>
#include <boost/operators.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>

namespace {

using namespace batt::int_types;

enum struct Player { X, O };
enum struct Square { Empty, X, O };

inline std::ostream& operator<<(std::ostream& out, const Square& t)
{
    switch (t) {
    case Square::Empty:
        return out << " ";
    case Square::X:
        return out << "X";
    case Square::O:
        return out << "O";
    }
    return out << "?";
}

struct GameState : boost::equality_comparable<GameState> {
    std::array<std::array<Square, 3>, 3> board;
    Player to_move;

    GameState() noexcept
        : board{{
              {{Square::Empty, Square::Empty, Square::Empty}},
              {{Square::Empty, Square::Empty, Square::Empty}},
              {{Square::Empty, Square::Empty, Square::Empty}},
          }}
        , to_move{Player::X}
    {
    }

    explicit GameState(std::array<std::array<Square, 3>, 3> b, Player p) noexcept : board{b}, to_move{p}
    {
    }

    batt::RadixQueue<64> pack() const
    {
        batt::RadixQueue<64> p;
        for (const auto& row : this->board) {
            for (const Square sq : row) {
                p.push(3, (int)sq);
            }
        }
        p.push(2, (int)this->to_move);

        return p;
    }

    static GameState unpack(batt::RadixQueue<64> q)
    {
        GameState g;
        for (auto& row : g.board) {
            for (Square& sq : row) {
                sq = (Square)q.pop(3);
            }
        }
        g.to_move = (Player)q.pop(2);
        return g;
    }

    struct Hash {
        usize operator()(const GameState& s) const
        {
            usize seed = 0;
            boost::hash_combine(seed, (int)s.to_move);
            for (const auto& row : s.board) {
                for (const auto& col : row) {
                    boost::hash_combine(seed, (int)col);
                }
            }
            return seed;
        }
    };

    friend bool operator<(const GameState& l, const GameState& r)
    {
        return l.board < r.board || (l.board == r.board && (l.to_move < r.to_move));
    }

    BATT_MAYBE_UNUSED friend std::ostream& operator<<(std::ostream& out, const GameState& t)
    {
        if (t.to_move == Player::X) {
            out << "(X to move)" << std::endl;
        } else {
            out << "(O to move)" << std::endl;
        }
        return out << t.board[0][0] << "|" << t.board[0][1] << "|" << t.board[0][2] << std::endl
                   << "-+-+-" << std::endl
                   << t.board[1][0] << "|" << t.board[1][1] << "|" << t.board[1][2] << std::endl
                   << "-+-+-" << std::endl
                   << t.board[2][0] << "|" << t.board[2][1] << "|" << t.board[2][2];
    }

    friend bool operator==(const GameState& l, const GameState& r)
    {
        return l.board == r.board && l.to_move == r.to_move;
    }
};

usize count(const GameState& s, Square v)
{
    usize n = 0;
    for (const auto& row : s.board) {
        for (const auto& col : row) {
            if (col == v) {
                n += 1;
            }
        }
    }
    return n;
}

GameState rotate90(const GameState& s)
{
    GameState s2 = s;
    for (usize i = 0; i < 3; ++i) {
        for (usize j = 0; j < 3; ++j) {
            s2.board[i][j] = s.board[j][2 - i];
        }
    }
    return s2;
}

GameState flipH(const GameState& s)
{
    GameState s2 = s;
    for (usize i = 0; i < 3; ++i) {
        for (usize j = 0; j < 3; ++j) {
            s2.board[i][j] = s.board[2 - i][j];
        }
    }
    return s2;
}

GameState flipV(const GameState& s)
{
    GameState s2 = s;
    for (usize i = 0; i < 3; ++i) {
        for (usize j = 0; j < 3; ++j) {
            s2.board[i][j] = s.board[i][2 - j];
        }
    }
    return s2;
}

Player flip_player(Player p)
{
    return p == Player::X ? Player::O : Player::X;
}

Square square_from_player(Player p)
{
    return p == Player::X ? Square::X : Square::O;
}

void for_each_symmetry(GameState s, std::function<void(const GameState& s)> fn)
{
    for (int r = 0; r < 4; ++r) {
        fn(s);
        fn(flipH(s));
        fn(flipV(s));
        s = rotate90(s);
    }
}

BATT_MAYBE_UNUSED Square get_winner(const GameState& s)
{
    for (usize i = 0; i < 3; ++i) {
        if (s.board[i][0] != Square::Empty && s.board[i][0] == s.board[i][1] &&
            s.board[i][1] == s.board[i][2]) {
            return s.board[i][0];
        }
        if (s.board[0][i] != Square::Empty && s.board[0][i] == s.board[1][i] &&
            s.board[1][i] == s.board[2][i]) {
            return s.board[0][i];
        }
    }

    // Check diagonals
    //
    if (s.board[1][1] != Square::Empty &&
        ((s.board[0][0] == s.board[1][1] && s.board[1][1] == s.board[2][2]) ||
         (s.board[2][0] == s.board[1][1] && s.board[1][1] == s.board[0][2]))) {
        return s.board[1][1];
    }

    return Square::Empty;
}

class TicTacToeModel : public batt::StateMachineModel<GameState, GameState::Hash>
{
   public:
    bool prune_won_games = true;
    bool prune_symmetries = true;

    GameState initialize() override
    {
        return GameState{/*board*/ {{
                             {{Square::Empty, Square::Empty, Square::Empty}},
                             {{Square::Empty, Square::Empty, Square::Empty}},
                             {{Square::Empty, Square::Empty, Square::Empty}},
                         }},
                         /*to_move*/ Player::X};
    }

    void set_state(const GameState& s) override
    {
        BATT_STATE_MACHINE_VERBOSE() << std::endl
                                     << "Evaluating state: " << std::endl
                                     << s << std::endl
                                     << s.pack();
        this->state_ = s;
    }

    void step() override
    {
        if (this->prune_won_games && get_winner(this->state_) != Square::Empty) {
            return;
        }
        BATT_STATE_MACHINE_VERBOSE() << " No winner.  Generating moves...";

        usize i = this->pick_int(0, 2);
        usize j = this->pick_int(100, 102) - 100;

        BATT_STATE_MACHINE_VERBOSE() << " Place at: (" << i << "," << j << ")";

        if (this->state_.board[i][j] == Square::Empty) {
            this->state_.board[i][j] = square_from_player(this->state_.to_move);
            this->state_.to_move = flip_player(this->state_.to_move);
            BATT_STATE_MACHINE_VERBOSE() << "-> " << this->state_;
        } else {
            BATT_STATE_MACHINE_VERBOSE() << " Illegal move.  Skipping...";
        }
    }

    GameState get_state() override
    {
        return this->state_;
    }

    bool check_invariants() override
    {
        usize ne = count(this->state_, Square::Empty);
        usize nx = count(this->state_, Square::X);
        usize no = count(this->state_, Square::O);

        return (ne + nx + no) == 9 && ((nx == no) || (nx == no + 1));
    }

    GameState normalize(const GameState& s) override
    {
        if (!this->prune_symmetries) {
            return s;
        }

        // All symmetries of a game state are equivalent, so pick the "least" one.
        //
        GameState s_norm = s;

        for_each_symmetry(s, [&](const GameState& t) {
            if (t < s_norm) {
                BATT_CHECK(!(s_norm < t) || s_norm == t);
                s_norm = t;
            }
        });
        if (s_norm != s) {
            BATT_STATE_MACHINE_VERBOSE() << " Normalized state: " << s_norm;
        }
        return s_norm;
    }

   private:
    GameState state_;
};

TEST(StateMachineModelTest, Basic)
{
    TicTacToeModel t;

    TicTacToeModel::Result r = t.check_model();

    std::vector<std::pair<int, int>> coords;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            coords.emplace_back(i, j);
        }
    }
    EXPECT_EQ(coords.size(), 9u);
    usize positions = 0;
    do {
        GameState g;
        auto g_packed = g.pack();
        for (const auto& p : coords) {
            g.board[p.first][p.second] = square_from_player(g.to_move);
            g.to_move = flip_player(g.to_move);
            EXPECT_TRUE(t.state_visited(t.normalize(g)))
                << g << " " << g_packed << " (" << p.first << "," << p.second << ")";
            g_packed = g.pack();
            if (t.prune_won_games && get_winner(g) != Square::Empty) {
                break;
            }
        }

        positions += 1;
    } while (std::next_permutation(coords.begin(), coords.end()));

    EXPECT_EQ(positions, usize(9 * 8 * 7 * 6 * 5 * 4 * 3 * 2 * 1));

    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.state_count, 764u);
    EXPECT_EQ(r.branch_count, 5805u);
    EXPECT_EQ(r.self_branch_count, 3535u);
}

}  // namespace
