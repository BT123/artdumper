// OATParser.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "OATParser.h"
#include "DexHeader.h"
#include "OATHeader.h"
#include "elfloader.h"

namespace Art {
	OATParser OATParser::m_oat_parser;

	OATParser::OATParser(const char* a_oat, unsigned int a_size) : m_oat_begin(a_oat){
		m_oat_end = a_oat + a_size;
	}

	OATParser::~OATParser() {
	}

	OATParser& OATParser::GetInstance() {
		return m_oat_parser;
	}

	void OATParser::init(const char* a_oat_file, const char* a_out_dex_path) {
		m_oat_file = a_oat_file;
		m_out_dex_path = a_out_dex_path;
	}

	const char* OATParser::Begin() {
		return m_oat_begin;
	}

	const char* OATParser::End() {
		return m_oat_end;
	}

	bool OATParser::OpenOat(std::unique_ptr<char[]>& a_buf, unsigned int& a_len)
	{
		bool ret = false;

		FILE* f = nullptr;
		errno_t e = fopen_s(&f, m_oat_file.c_str(), "rb+");
		if (e != 0) {
			return ret;
		}

		fseek(f, 0, SEEK_END);
		a_len = ftell(f);

		fseek(f, 0, SEEK_SET);
		a_buf.reset(new char[a_len]);

		fread_s(a_buf.get(), a_len, sizeof(char), a_len, f);
		ret = true;

		return ret;
	}

	bool OATParser::OatReSet(std::unique_ptr<char[]>& a_buf) {
		unsigned int offset = 0;
		unsigned int len = 0;

		bool ret = GetOatInfo(offset, len);

		m_oat_begin = a_buf.get() + offset;
		m_oat_end = a_buf.get() + offset + len;

		m_oatheader.reset(new OATHeader());
		memcpy_s(m_oatheader.get(), sizeof(OATHeader), m_oat_begin, sizeof(OATHeader));

		return ret;
	}

	bool OATParser::Parser() {
		std::unique_ptr<char[]> buf;
		unsigned int oat_len = 0;

		// ��oat�ļ�
		if (!OpenOat(buf, oat_len)) {
			return false;
		}

		// ����elfģ���ȡrodata������ʼ��
		if (!ElfInit(buf.get(), oat_len)) {
			return false;
		}

		if (!OatReSet(buf)) {
			return false;
		}

		// �ļ�ͷ�Ǵ�oat���ļ�ͷ���Ǵ�rodata����ʼ
		const char* oat = Begin();

		// OatHeader��ͷΪ0x54
		oat += sizeof(OATHeader);
		if (oat > End()) {
			return false;
		}

		// ����һЩkey-value�Ĵ洢ֵ
		oat += GetOatHeader()->GetKeyValueStoreSize();
		if (oat > End()) {
			return false;
		}

		// ��ͷ��ƫ��0x14����ȡdex�ĸ���
		uint32_t dex_file_count = GetOatHeader()->GetDexFileCount();
		for (size_t i = 0; i < dex_file_count; i++) {
			// ��ȡ�����jar������dex�����ֵĳ��ȣ� ����21 00 00 00
			uint32_t dex_file_location_size = *reinterpret_cast<const uint32_t*>(oat);

			if (dex_file_location_size == 0U) {
				return false;
			}

			// ���������jar������dex���ַ�����������/system/framework/core-libart.jar
			oat += sizeof(dex_file_location_size);
			if (oat > End()) {
				return false;
			}

			// ��ȡ�����jar������dex���ļ���
			const char* dex_file_location_data = reinterpret_cast<const char*>(oat);
			oat += dex_file_location_size;
			if (oat > End()) {
				return false;
			}

			// ���ݳ��Ⱥ��ַ�����ֵ��string�����ַ���
			std::string dex_file_location(dex_file_location_data, dex_file_location_size);

			// �����ļ�У��
			uint32_t dex_file_checksum = *reinterpret_cast<const uint32_t*>(oat);
			oat += sizeof(dex_file_checksum);
			if (oat > End()) {
				return false;
			}

			// ��ȡ����dex����jar����ƫ������ƫ���������rodata��  ���� 04 DA 00 00
			uint32_t dex_file_offset = *reinterpret_cast<const uint32_t*>(oat);
			oat += sizeof(dex_file_offset);

			const char* dex_file_pointer = Begin() + dex_file_offset;
			const DexHeader* header = reinterpret_cast<const DexHeader*>(dex_file_pointer);

			if (!Dump(dex_file_location, header)) {
				return false;
			}

			oat += (sizeof(uint32_t) * header->class_defs_size_);
		}

		return true;
	}

	void OATParser::MakeDexName(std::string& a_dex_name, std::string& a_out_dex_name) {
		size_t pos = a_dex_name.find_last_of('/');
		a_out_dex_name = m_out_dex_path + "\\" + a_dex_name.substr(++pos);
	}

	bool OATParser::Dump(std::string& a_dex_name, const DexHeader* a_dex_header) {
		bool ret = false;

		FILE* f = nullptr;
		std::string out_dex_name;
		MakeDexName(a_dex_name, out_dex_name);

		errno_t e = fopen_s(&f, out_dex_name.c_str(), "ab+");
		if (e == 0) {
			fwrite(a_dex_header, sizeof(char), a_dex_header->file_size_, f);
			fclose(f);

			ret = true;
		}
		else {
			printf_s("error is %d", e);
		}

		return ret;
	}
}

// ��ʼ���� oat_fileΪԭʼ��oat�ļ��� out_dex_pathΪ�����dex·��
extern "C" __declspec(dllexport) bool InitOatParser(const char* oat_file, const char* out_dex_path) {
	Art::OATParser::GetInstance().init(oat_file, out_dex_path);
	return true;
}

// ��Ota�ļ�dump��dex�ļ�
extern "C" __declspec(dllexport) bool DoDumpToDex() {
	return Art::OATParser::GetInstance().Parser();
}