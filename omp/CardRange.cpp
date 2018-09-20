#include "CardRange.h"
#include "Constants.h"
#include "Util.h"
#include "Hand.h"

#include <locale>
#include <algorithm>
#include <cassert>
#include <string>

namespace omp {

// Construct empty.
CardRange::CardRange()
{
}

// Construct from expression.
CardRange::CardRange(const std::string& text)
{
    // Turn to lowercase and remove spaces and control chars.
    std::locale loc;
    std::string s;
    for (char c: text) {
        if (std::isgraph(c, loc))
            s.push_back(std::tolower(c, loc));
    }

    const char* p = s.data();
    while(parseHand(p) && parseChar(p, ','))
          ;

    if (s == "random")
        addAll();

    removeDuplicates();
}

CardRange::CardRange(const char* text)
    : CardRange(std::string(text))
{
}

// Construct from vctor.
CardRange::CardRange(const std::vector<std::array<uint8_t,2>>& combos)
{
    for (auto& combo : combos)
        addCombo(combo[0], combo[1]);
    removeDuplicates();
}

// Card mask from a string.
uint64_t CardRange::getCardMask(const std::string& text)
{
    std::locale loc;
    std::string s;
    for (char c: text) {
        if (std::isgraph(c, loc))
            s.push_back(std::tolower(c, loc));
    }

    uint64_t cards = 0;
    for (size_t i = 0; i < s.size() - std::min<size_t>(1, s.size()); i += 2) {
        unsigned rank = charToRank(s[i]);
        unsigned suit = charToSuit(s[i + 1]);
        if (rank == ~0u || suit == ~0u)
            break;
        unsigned card = 4 * rank + suit;
        cards |= 1ull << card;
    }

    return cards;
}

std::string CardRange::cardMaskToStr(uint64_t mask)
{
    std::string cards;
    for (unsigned i=0; mask; mask >>= 1, ++i){
        if (1ull & mask) {
            // enable to get comma-separated cards
            // if (!cards.empty())
            //     cards.push_back(',');

            cards += cardNumberToStr(i);
        }
    }
    return cards;
}

std::string CardRange::handToStr(const Hand hand)
{
    return handMaskToStr(hand.mask());
}

std::string CardRange::handMaskToStr(uint64_t mask)
{
    // the masks are encoded with:
    //  1ull << ((3 - suit) * 16 + rank)
    // ... so:
    // 00 -> 3*16 + 0 == 48
    // 11 -> 2*16 + 1 == 33
    // 30 -> 0*16 + 0 == 0

    // we convert the handMask to a cardMask
    uint64_t cardMask = 0ull;
    for (unsigned i=0; mask; mask >>= 1, ++i){
        if (1ull & mask) {
            unsigned rank = i % 16;
            unsigned suit = ~(i / 16) & 3ull;

            unsigned card = 4 * rank + suit;
            cardMask |= 1ull << card;
        }
    }
    return cardMaskToStr(cardMask);
}

std::string CardRange::cardNumberToStr(unsigned c)
{
    unsigned rank = c / 4;
    unsigned suit = c % 4;
    std::string s;
    s.push_back(rankToChar(rank));
    s.push_back(suitToChar(suit));
    return s;
}

unsigned CardRange::strToCardNumber(std::string card)
{
    unsigned rank = charToRank(card[0]);
    unsigned suit = charToSuit(card[1]);
    return 4 * rank + suit;
}


// Parses a single hand and advances pointer p.
bool CardRange::parseHand(const char*&p)
{
    const char* backtrack = p;

    bool explicitSuits = false;
    unsigned r1, r2, s1, s2;
    if (!parseRank(p, r1))
        return false;
    explicitSuits = parseSuit(p, s1);
    if (!parseRank(p, r2)) {
        p = backtrack;
        return false;
    }
    if (explicitSuits && !parseSuit(p, s2)) {
        p = backtrack;
        return false;
    }

    if (explicitSuits) {
        unsigned c1 = 4 * r1 + s1, c2 = 4 * r2 + s2;
        if (c1 == c2) {
            p = backtrack;
            return false;
        }
        addCombo(c1, c2);
    } else if (!explicitSuits) {
        bool suited = true, offsuited = true;
        if (parseChar(p, 'o'))
            suited = false;
        else if (parseChar(p, 's'))
            offsuited = false;
        if (parseChar(p, '+'))
            addCombosPlus(r1, r2, suited, offsuited);
        else
            addCombos(r1, r2, suited, offsuited);
    }

    return true;
}

// Parse a rank from 2 to A.
bool CardRange::parseRank(const char*&p, unsigned& rank)
{
    rank = charToRank(*p);
    if (rank == ~0u)
        return false;
    ++p;
    return true;
}

// Parse a suit (c, d, h, s)
bool CardRange::parseSuit(const char*&p, unsigned& suit)
{
    suit = charToSuit(*p);
    if (suit == ~0u)
        return false;
    ++p;
    return true;
}

// Try to read a specific character.
bool CardRange::parseChar(const char*&p, char c)
{
    if (*p == c) {
        ++p;
        return true;
    } else {
        return false;
    }
}

// Add combos for specific ranks.
void CardRange::addCombos(unsigned rank1, unsigned rank2, bool suited, bool offsuited)
{
    if (suited && rank1 != rank2) {
        for (unsigned suit = 0; suit < 4; ++suit)
            addCombo(4 * rank1 + suit, 4 * rank2 + suit);
    }
    if (offsuited) {
        for (unsigned suit1 = 0; suit1 < 4; ++suit1)
            for (int suit2 = suit1 + 1; suit2 < 4; ++suit2) {
                addCombo(4 * rank1 + suit1, 4 * rank2 + suit2);
                if (rank1 != rank2)
                    addCombo(4 * rank1 + suit2, 4 * rank2 + suit1);
            }
    }
}

// Add range of hands defined by the "+" suffix.
void CardRange::addCombosPlus(unsigned rank1, unsigned rank2, bool suited, bool offsuited)
{
    if (rank1 == rank2){
        for (unsigned r = rank1; r < 13; ++r)
            addCombos(r, r, suited, offsuited);
    } else {
        if (rank1 < rank2)
            std::swap(rank1, rank2);
        for (unsigned r = rank2; r < rank1; ++r)
            addCombos(rank1, r, suited, offsuited);
    }
}

void CardRange::addAll()
{
    for (unsigned c1 = 0; c1 < CARD_COUNT; ++c1)
        for (unsigned c2 = 0; c2 < c1; ++c2)
            addCombo(c1, c2);
}

void CardRange::addCombo(unsigned c1, unsigned c2)
{
    omp_assert(c1 != c2);
    if (c1 >> 2 < c2 >> 2 || (c1 >> 2 == c2 >> 2 && (c1 & 3) < (c2 & 3)))
        std::swap(c1, c2);
    mCombinations.push_back({(uint8_t)c1, (uint8_t)c2});
}

// Removes duplicate combos.
void CardRange::removeDuplicates()
{
    std::sort(mCombinations.begin(), mCombinations.end(), [](const std::array<uint8_t,2>& lhs,
              const std::array<uint8_t,2>& rhs){
        if (lhs[0] >> 2 != rhs[0] >> 2)
            return lhs[0] >> 2 < rhs[0] >> 2;
        if (lhs[1] >> 2 != rhs[1] >> 2)
            return lhs[1] >> 2 < rhs[1] >> 2;
        if ((lhs[0] & 3) != (rhs[0] & 3))
            return (lhs[0] & 3) < (rhs[0] & 3);
        return (lhs[1] & 3) < (rhs[1] & 3);
    });
    auto last = std::unique(mCombinations.begin(), mCombinations.end(), [](const std::array<uint8_t,2>& lhs,
                            const std::array<uint8_t,2>& rhs){
        return lhs[0] == rhs[0] && lhs[1] == rhs[1];
    });
    mCombinations.erase(last, mCombinations.end());
}

unsigned CardRange::charToRank(char c)
{
    switch(c) {
        case 'a': return 12;
        case 'k': return 11;
        case 'q': return 10;
        case 'j': return 9;
        case 't': return 8;
        case '9': return 7;
        case '8': return 6;
        case '7': return 5;
        case '6': return 4;
        case '5': return 3;
        case '4': return 2;
        case '3': return 1;
        case '2': return 0;
        // capitals, to maintain isomorphism with rankToChar
        case 'A': return 12;
        case 'K': return 11;
        case 'Q': return 10;
        case 'J': return 9;
        case 'T': return 8;
        default: return ~0u;
    }
}

char CardRange::rankToChar(unsigned c)
{
    switch(c) {
        case 12: return 'A';
        case 11: return 'K';
        case 10: return 'Q';
        case 9: return 'J';
        case 8: return 'T';
        case 7: return '9';
        case 6: return '8';
        case 5: return '7';
        case 4: return '6';
        case 3: return '5';
        case 2: return '4';
        case 1: return '3';
        case 0: return '2';
        default: return '?';
    }
}

unsigned CardRange::charToSuit(char c)
{
    switch(c) {
        case 'c': return 0;
        case 'd': return 1;
        case 'h': return 2;
        case 's': return 3;
        default: return ~0u;
    }
}

char CardRange::suitToChar(unsigned c)
{
    switch(c) {
        case 0: return 'c';
        case 1: return 'd';
        case 2: return 'h';
        case 3: return 's';
        default: return '?';
    }
}

}
