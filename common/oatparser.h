#pragma once

// ��ʼ���� oat_fileΪԭʼ��oat�ļ��� out_dex_pathΪ�����dex·��
extern "C" __declspec(dllimport) bool InitOatParser(const char* oat_file, const char* out_dex_path);

// ��Ota�ļ�dump��dex�ļ�
extern "C" __declspec(dllimport) bool DoDumpToDex();

