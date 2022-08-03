#!./apl --script
 ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝
⍝                                                                    ⍝
⍝ dump_load_SI.apl                     2021-01-19  22:13:50 (GMT+1)  ⍝
⍝                                                                    ⍝
 ⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝⍝

∆length←00 00
    ((⎕IO+0)⊃∆length)←'meter'
    ((⎕IO+1)⊃∆length)←2 5⍴00 00 00 00 00 00,'v∘∘∘'
      ((⎕IO+1 (0 0))⊃∆length)←'dc'
      ((⎕IO+1 (0 1))⊃∆length)←'meter'
      ((⎕IO+1 (0 2))⊃∆length)←'km'
      ((⎕IO+1 (0 3))⊃∆length)←'lightyear'
      ((⎕IO+1 (0 4))⊃∆length)←'parsec'
      ((⎕IO+1 (1 0))⊃∆length)←(,⎕UCS 118 247),'10'
      ((⎕IO+1 (1 2))⊃∆length)←(,⎕UCS 118 215),'1000'
      ((⎕IO+1 (1 3))⊃∆length)←(,⎕UCS 118 215),'9454254955488000'
      ((⎕IO+1 (1 4))⊃∆length)←(,⎕UCS 118 215),'3.0856776E16'

⎕CT←1E¯13

⎕FC←(,⎕UCS 46 44 8902 48 95 175)

⎕IO←1

⎕L←0

⎕LX←' ' ⍝ proto 1
  ⎕LX←0⍴⎕LX ⍝ proto 2

⎕PP←10

⎕PR←' '

⎕PS←0 0

⎕PW←80

⎕R←0

⎕RL←16807

⎕TZ←1

⎕X←0
