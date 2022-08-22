#include "ScoreRuler.h"
#include "util.h"
#include "Rcpp.h"

using namespace Rcpp;

ScoreRuler::ScoreRuler(const std::vector<std::vector<double> > & inpE,
                       unsigned inpSampleSize, unsigned inpGenesetSize):
    expressionMatrix(inpE), sampleSize(inpSampleSize), genesetSize(inpGenesetSize){
    currentSample.resize(inpSampleSize), currentProfiles.resize(inpSampleSize);
}



ScoreRuler::~ScoreRuler() = default;


void ScoreRuler::duplicateSampleElements(){
    /*
     * Replaces sample elements that are less than median with elements
     * that are greater that the median
     */

    std::vector<std::pair<double, unsigned> > scoreAndIndex(sampleSize);

    for (unsigned elemIndex = 0; elemIndex < sampleSize; elemIndex++) {
        double elemScore = getScore(currentProfiles[elemIndex]);
        scoreAndIndex[elemIndex] = std::make_pair(elemScore, elemIndex);
    }
    std::sort(scoreAndIndex.begin(), scoreAndIndex.end());

    for (unsigned elemIndex = 0; 2 * elemIndex < sampleSize; elemIndex++){
        scores.push_back(scoreAndIndex[elemIndex].first);
    }

    std::vector<std::vector<unsigned> > tempSample;
    std::vector<std::vector<double> > tempProfiles;
    for (unsigned elemIndex = 0; 2 * elemIndex < sampleSize - 2; elemIndex++) {
        for (unsigned rep = 0; rep < 2; rep++) {
            tempSample.push_back(currentSample[scoreAndIndex[sampleSize - 1 - elemIndex].second]);
            tempProfiles.push_back(currentProfiles[scoreAndIndex[sampleSize - 1 - elemIndex].second]);
        }
    }
    tempSample.push_back(currentSample[scoreAndIndex[sampleSize >> 1].second]);
    tempProfiles.push_back(currentProfiles[scoreAndIndex[sampleSize >> 1].second]);
    std::swap(currentSample, tempSample);
    std::swap(currentProfiles, tempProfiles);
}

void ScoreRuler::extend(double inpScore, int seed, double eps) {
    std::mt19937 mtGen(seed);

    // fill currentSample
    for (unsigned elemIndex = 0; elemIndex < sampleSize; elemIndex++) {
        std::vector<int> comb = combination(0, expressionMatrix.size() - 1, genesetSize, mtGen);
        currentSample[elemIndex] = std::vector<unsigned>(comb.begin(), comb.end());
        currentProfiles[elemIndex] = getProfile(expressionMatrix, currentSample[elemIndex]);
    }

    duplicateSampleElements();

    while (scores.back() <= inpScore - 1e-10){
        for (int moves = 0; moves < sampleSize * genesetSize;) {
            for (unsigned elemIndex = 0; elemIndex < sampleSize; elemIndex++) {
                moves += updateElement(currentSample[elemIndex], currentProfiles[elemIndex],
                                       scores.back(), mtGen);
            }
        }

        duplicateSampleElements();
        if (eps != 0){
            unsigned long k = scores.size() / ((sampleSize + 1) / 2);
            if (k > - log2(0.5 * eps)) {
                break;
            }
        }
    }
}

double ScoreRuler::getPvalue(double inpScore, double eps){
    unsigned long halfSize = (sampleSize + 1) / 2;

    auto it = scores.begin();
    if (inpScore >= scores.back()){
        it = scores.end() - 1;
    }
    else{
        it = lower_bound(scores.begin(), scores.end(), inpScore);
    }

    unsigned long indx = 0;
    (it - scores.begin()) > 0 ? (indx = (it - scores.begin())) : indx = 0;

    unsigned long k = (indx) / halfSize;
    unsigned long remainder = sampleSize -  (indx % halfSize);

    double adjLog = betaMeanLog(halfSize, sampleSize);
    double adjLogPval = k * adjLog + betaMeanLog(remainder + 1, sampleSize);
    return std::max(0.0, std::min(1.0, exp(adjLogPval)));
}


int ScoreRuler::updateElement(std::vector<unsigned> & element,
                              std::vector<double> & profile,
                              double threshold,
                              std::mt19937 &mtGen){
    double upPrmtr = 0.1;
    unsigned n = expressionMatrix.size();

    uid_wrapper uid_n(0, n - 1, mtGen);
    uid_wrapper uid_k(0, genesetSize - 1, mtGen);

    unsigned niters = std::max(unsigned(1), unsigned(genesetSize * upPrmtr));
    int moves = 0;
    std::vector<double> newProfile(profile.size());
    for (unsigned i = 0; i < niters; i++){
        unsigned toDrop = uid_k();
        unsigned indxOld = element[toDrop];
        unsigned indxNew = uid_n();

        if (find(element.begin(), element.end(), indxNew) != element.end()){
            continue;
        }

        adjustProfile(expressionMatrix, profile, newProfile, indxNew, indxOld);
        double newScore = getScore(newProfile);

        if (newScore >= threshold){
            element[toDrop] = indxNew;
            std::swap(profile, newProfile);
            moves++;
        }

    }
    return moves;
}