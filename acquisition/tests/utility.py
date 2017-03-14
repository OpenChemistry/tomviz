def tobytes(img, format='TIFF'):
    try:
        import StringIO
        buf = StringIO.StringIO()
    except ImportError:
        import io
        buf = io.BytesIO()

    img.save(buf, format)

    return buf.getvalue()
