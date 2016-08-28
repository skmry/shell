# coding:UTF-8

import os
import copy

trapList = []

if __name__ == "__main__":
    f = open("D:\user.def", "r")

    enterprise = ""
    generic = ""
    specific = ""
    node = ""
    summary = ""
    detail = ""
    action = ""
    recoveryNo = ""
    recoveryCondition = ""
    print enterprise

    for row in f:
        row = row.strip("\r").strip("\n").strip("\r\n").strip()
        if row.startswith("Enterprise"):
            enterprise = row.lstrip("Enterprise:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("GenericCode"):
            generic = row.lstrip("GenericCode:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("SpecificCode"):
            specific = row.lstrip("SpecificCode:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("Node"):
            node = row.lstrip("Node:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("Action"):
            action = row.lstrip("Action:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("Summary"):
            summary = row.lstrip("Summary:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("Detail"):
            detail = row.lstrip("Detail:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("RecoveryNo"):
            recoveryNo = row.lstrip("RecoveryNo:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        elif row.startswith("RecoveryCondition"):
            recoveryCondition = row.lstrip("RecoveryCondition:").strip().rstrip("\r").rstrip("\r\n").rstrip("\n")
        else:
            if enterprise != "" and generic != "" and specific != "":
                print str(len(enterprise))
                if row == "\r\n" or row == "\r" or row == "\n" or row == "":
                    if node == "":
                        node = "*"
                    trapList.append([enterprise, generic, specific, node, summary, detail, action, recoveryNo, recoveryCondition])
                enterprise = ""
                generic = ""
                specific = ""
                node = ""
                summary = ""
                detail = ""
                action = ""
                recoveryNo = ""
                recoveryCondition = ""
            else:
                enterprise = ""
                generic = ""
                specific = ""
                node = ""
                summary = ""
                detail = ""
                action = ""
                recoveryNo = ""
                recoveryCondition = ""

    for x in trapList:
        print x

    s = set(trapList)
    print len(s)
