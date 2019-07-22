# gossip-membership-protocol
Implementation of a gossip style membership/dissemination protocol that detect failures and stays up to date

Based on the Cloud Computing Concepts course offered by Prof. Indranil Gupta from University of Illinois at Urbana-Champaign/Coursera.

## Compilation 
`make`

## Running the code
`./Grader.sh`

## To run individual test cases
`./Application testcases/<conf-file>`

## Criteria the protocol aims to satisfy:

1. Completeness all the time: every non-faulty process must detect every node join, failure, and leave, and
2. Accuracy of failure detection when there are no message losses and message delays are small
When there are message losses, completeness must be satisfied and accuracy must be high. It must achieve all of these even under simultaneous multiple failures.
