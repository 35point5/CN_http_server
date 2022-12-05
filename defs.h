//
// Created by 46473 on 2022/11/26.
//

#ifndef CN_LAB7_SOCKET_DEFS_H
#define CN_LAB7_SOCKET_DEFS_H
#define TO_STRING(x) (string((((char*)&x)),sizeof(x)))
enum Special{
    BeginPackage = 0x23,
    EndPackage = 0x66,
    Trans = 0x88
};
enum OpType{
    Get,
    Post
};
enum Stage{
    MethodStage,
    HeaderStage,
    ContentStage
};
const int buf_size=4096;
const std::string host_name="Mogician";
const std::string prefix="D:/git/web/";
#endif //CN_LAB7_SOCKET_DEFS_H
