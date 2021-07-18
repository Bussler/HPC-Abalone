#include "search.h"
#include "board.h"
#include "eval.h"
#include <stdio.h>
#include <omp.h>
#include <algorithm>
#include <cstring>

class MinimaxStrategy: public SearchStrategy
{
 public:
    MinimaxStrategy(): SearchStrategy("Minimax") {}

    SearchStrategy* clone() { return new MinimaxStrategy(); }

 private:

    /**
     * Implementation of the strategy.
     */
    void searchBestMove();

    bool movesEqual(Move& m1, Move& m2);
    
    Variation _pv; 
    Move mList[10]; //best move list in last search
    bool _inPv;
    bool _foundBestFromPrev;
    Move _currentBestMove;

    int doMinMaxSearch(int depth, Board& board, Evaluator& evaluator, Move* mlist, int alpha, int beta, int depthOfPv, bool pushParallel, int curMaxdepth);
    float doMinMaxSearchSeq(int depth, int alpha, int beta, Move* mlist);

    int threadsReady = 48;
    bool reachedBottom=false;

    int upperAlphaArray[10];
    omp_lock_t lockArray[10];
};

bool MinimaxStrategy::movesEqual(Move& m1, Move& m2){
    if (m1.field != m2.field)
        return false;

    /* if direction is supplied it has to match */
    if ((m1.direction > 0) && (m1.direction != m2.direction))
        return false;

    /* if type is supplied it has to match */
    if ((m1.type != Move::none) && (m1.type != m2.type))
        return false;

    return true;
}

float MinimaxStrategy::doMinMaxSearchSeq(int depth, int alpha, int beta, Move* mlist){
    float maxEval = -999999;

    if (depth >= SearchStrategy::_maxDepth)
    { //stop search here
        return evaluate();
    }

    Move m;
    MoveList list;

    generateMoves(list); //generate currently possible moves

    while (list.getNext(m))
    { //iterate through each possible move
        playMove(m);
        int eval;
        if (depth + 1 < SearchStrategy::_maxDepth) {
            eval = -doMinMaxSearchSeq(depth + 1, -beta, -alpha, mlist); //call for the enemy here, so change sign to get the best move for us
        } else {
            eval = evaluate();
        }
        takeBack();

        if (eval > maxEval)
        {
            maxEval = eval;
            //safe m -> do own variation pv?
            //_pv.update(depth, m);
            mlist[depth] = m;
            foundBestMove(depth, m, eval);

            if (depth == 0)
            { //if we are at start of tree set move as nex best
                _currentBestMove = m;
                std::memcpy(mList, mlist, sizeof(Move)*10);
            }

        }

        //alpha beta pruning
        if (eval > alpha)
        {
            alpha = eval;
        }

        if (beta <= alpha)
        {
            break;
        }
    }

    //printf("Maxdeval at rank: %d: %d\n", depth, maxEval);

    finishedNode(depth, _pv.chain(depth));
    return maxEval;
}

int MinimaxStrategy::doMinMaxSearch(int depth, Board& board, Evaluator& evaluator, Move* mlist, int alpha, int beta, int depthOfPv, bool pushParallel, int curMaxdepth){
    int someVal = -999999;
    int* maxEval = &someVal;

    Move m;
    Move nodeBestMove;
    MoveList list;

    board.generateMoves(list); //generate currently possible moves

    bool arePv = !reachedBottom;
    if(arePv)
    {
        upperAlphaArray[depth] = alpha;
        depthOfPv=depth;
    }
        

    if(_inPv){ //check move in pv first to get best moves
        if(depth+2 < 10){ // offset 2: we and enemy already played last turn
            m = mList[depth+2];
        }

        if(m.type != Move::none && !list.isElement(m,0,true)){ // if pv move is not possible set to null in order to stop pv search and get next from list in loop
            m.type = Move::none;
        }

        if(m.type == Move::none){ //handle cases in which we are too deep already or didn't find pv move in possible list: stop pv
            #pragma omp critical (update_inPv)
            {
                _inPv = false;
            }
                
        }
    }


    while(true) { //iterate through each possible move

        if(m.type == Move::none){ //get next move if not yet taken from pv
            if(!list.getNext(m))
                break;
        }

        bool createThread = false;

        if(arePv && reachedBottom && (depth < curMaxdepth - 2)){ //PV-Splitting: Only create threads on PV nodes
            createThread = true;
        }

        if(!createThread) { //handle sequentially to safe on copy operations

            board.playMove(m);
            int eval;
            if (depth + 1 < curMaxdepth) //SearchStrategy::_maxDepth
            {
                eval = -doMinMaxSearch(depth + 1, board, evaluator, mlist, -beta, -alpha, depthOfPv, pushParallel, curMaxdepth); //call for the enemy here, so change sign to get the best move for us
            }
            else
            {
                reachedBottom = true;
                eval = evaluator.calcEvaluation(&board);
            }
            board.takeBack();

            if (eval > *maxEval)
            {
                *maxEval = eval;
                //safe m -> do own variation pv?
                mlist[depth] = m;
                foundBestMove(depth, m, eval);

                if (depth == 0)
                { //if we are at start of tree set move as nex best
                    _currentBestMove = m;
                }
            }


            //omp_set_lock(&(lockArray[depthOfPv]));
            if (!arePv)
            {   
                    int upperAlpha = upperAlphaArray[depthOfPv];

                    if ((depth - depthOfPv) % 2 == 0) // TODO ohboi this could be trouble. In case of bugs look here
                    {
                        if (upperAlpha > alpha)
                        {
                            alpha = upperAlpha;
                        }
                    }
                    else
                    {
                        if (-upperAlpha < beta)
                        {
                            beta = -upperAlpha;
                        }
                    }
                
            }

            if (eval > alpha)
            {
                alpha = eval;
            }

            if (beta <= alpha)
            {
                break;
            }

        }
        else { //parallel: create tasks on lower depths

            bool breakLoop = false;
            #pragma omp task firstprivate(m, depth, board, maxEval, evaluator, depthOfPv, pushParallel)
            {   

                Move newTaskMList[10];//children write here

                board.playMove(m);
                int eval;
                if (depth + 1 < curMaxdepth)
                {
                    eval = -doMinMaxSearch(depth + 1, board, evaluator, newTaskMList, -beta, -alpha, depthOfPv, pushParallel, curMaxdepth); //call for the enemy here, so change sign to get the best move for us
                }
                else
                {
                    eval = evaluator.calcEvaluation(&board);
                }

                board.takeBack();

                omp_set_lock(&(lockArray[depthOfPv]));
                if (eval > *maxEval)
                {
                    *maxEval = eval;

                    //safe m -> do own variation pv?
                    newTaskMList[depth] = m;
                    foundBestMove(depth, m, eval);

                    //copy everything from TaskMList to mlist
                    for(int d = depth; d < curMaxdepth; d++){
                    mlist[d] = newTaskMList[d];
                    }

                    if (depth == 0)
                    { //if we are at start of tree set move as nex best
                        _currentBestMove = m;
                    }
                }

                if (eval > alpha)
                {
                    alpha = eval;
                    if (arePv)
                    {
                        upperAlphaArray[depthOfPv] = eval;
                    }
                }

                if (beta <= alpha)
                {
                    breakLoop = true;
                }
                omp_unset_lock(&(lockArray[depthOfPv]));

                
                if (!arePv)
                {   

                    int upperAlpha = upperAlphaArray[depthOfPv];

                    if ((depth - depthOfPv) % 2 == 0) 
                    {
                        if (upperAlpha > alpha)
                        {
                            alpha = upperAlpha;
                        }
                    }
                    else
                    {
                        if (-upperAlpha < beta)
                        {
                            beta = -upperAlpha;
                        }
                    }
                }

            }

            if(breakLoop)
                break;

        }

        m.type = Move::none;
    }
    
    #pragma omp taskwait 
    
    if(depth == 0){
        std::memcpy(mList, mlist, sizeof(Move) * 10); // copy last pv into global pv to store
    }
    //printf("Maxdeval at rank: %d: %d\n", depth, *maxEval);
    //finishedNode(depth, _pv.chain(depth));
    return *maxEval;
}


void MinimaxStrategy::searchBestMove()
{
    reachedBottom=false;

    for (int i=0; i<10; i++)
        omp_init_lock(&(lockArray[i]));
    
    if(mList[0].type != Move::none) // we have something cached
        _inPv = true;

    omp_set_num_threads(48); //Change if locally
    Move newMList[10];
    int test;

    #pragma omp parallel
    {
        #pragma omp single
        test = doMinMaxSearch(0, *_board, *_ev, newMList, -15000, 15000, 0, false, SearchStrategy::_maxDepth);
    }

    //test = doMinMaxSearchSeq(0, -15000, 15000, mList);
    printf("maxEval = %d \n", test);

    _bestMove = _currentBestMove; //update _bestmove

    for (int i=0; i<10; i++)
        omp_destroy_lock(&(lockArray[i]));

    printf("Output Principal Variation:\n");
    for (int i = 0; i < SearchStrategy::_maxDepth; i++)
    {
        mList[i].print();
        printf("\n");
    }
    

}

// register ourselve as a search strategy
MinimaxStrategy minimaxStrategy;
