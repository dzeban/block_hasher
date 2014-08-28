block_hasher
============

Simple tool that tests integrity and performance of block device.

It reads block device in multiple threads in blocks while calculating hash sums and measuring time. 
In the end it will print nice text report like this:

    T04: 23.35 MB/s 6f43a38e979e053adba5be589803e1d21c3efc00
    T09: 23.21 MB/s 0a912bf91ab6aae3118e21c18c80e15cf056db00
    T06: 23.19 MB/s 0d8b5d7cee3bec090ba3a9c7768c05129b394300
    T08: 23.17 MB/s 45e80a3731669b0b549e6d98380dc1314321fb00
    T03: 23.16 MB/s 9a39578dd1b35f873b45121c9f09c87b6e1d9400
    T01: 23.15 MB/s 933a1fba87f333f7bf5f2423cd60500874a8c100
    T02: 23.11 MB/s 2b30307b379311ebb352fbbc6fde42daea385000
    T00: 23.10 MB/s acd0168becc29812d6824763c7a43edbab6f4a00
    T07: 23.10 MB/s 82c5decfb0503cb87c806ca1e6fe7c0ecd0c4b00
    T05: 23.08 MB/s 2d2ee48e62882516e6afe1bb30dc3ef920f09600

I use this tool as profiling subject in my Linux profiling articles at 
<http://avd.reduct.ru/programming/profiling-intro.html>
