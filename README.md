* What's done:
1. Communications (username, connect, join room, send line, like/unlike line).
2. Partition tolerance with total ordering.
See the design doc for more information.

* Bugs:
1. When remerge, the newly added likes may be in the different position, thus causing an inconsistency. (I feel that we can't simply put line number in the like/unlike line update because when the servers remerge the lines get different indices. Sadly we haven't found a way to get around this.)
2. When two servers are split away and send some lines and likes, if likes arrive first, then the likes will not be properly added. (We should have solved this but we didn't, but we created a Map and wish to map the line index to like count and append likes later when the line is added instead of creating a dummy line. We'll show you.)
3. When there are too many history records, they will be sent through multiple replies. The reply after the second reply will not be properly displayed. (Memory alignment issue? But the history need to > 100 in order to be sent through multiple replies, so you don't really see it when you are testing.)
4. (Minor) the client display is a little bit weird ([> Username] prompt comes before log).
