
# -*- coding: utf-8 -*-
import webbrowser
import PySimpleGUI as sg
import speech_recognition as sr
import pyttsx3
import os
from time import sleep

# -----------------------------------------
# CONFIGURACIÓN DE VOZ - Tuve que ajustar esto varias veces
# -----------------------------------------

def hablar(texto):
    """
    Hace que la compu hable - a veces se traba con textos largos
    """
    try:
        motor_voz = pyttsx3.init()
        
        # Configurar propiedades de voz (esto lo fui agregando después)
        voces = motor_voz.getProperty('voices')
        if len(voces) > 0:
            motor_voz.setProperty('voice', voces[0].id)  # Usar primera voz disponible
        
        motor_voz.setProperty('rate', 150)  # Velocidad de habla
        motor_voz.say(texto)
        motor_voz.runAndWait()
        
    except Exception as error:
        print(f"Ups, error con la voz: {error}")

def escuchar():
    """
    Escucha lo que digo por micrófono - a veces no funciona bien
    """
    reconocedor = sr.Recognizer()
    
    # Tuve que agregar esto porque el micrófono no siempre funciona a la primera
    try:
        with sr.Microphone() as mic:
            print("Escuchando... (di algo)")
            hablar("Te estoy escuchando, decime")
            
            # Ajustar para ruido ambiente - esto mejoró mucho el reconocimiento
            reconocedor.adjust_for_ambient_noise(mic, duration=0.5)
            reconocedor.pause_threshold = 1.0  # Esperar un poco más
            
            audio = reconocedor.listen(mic, timeout=10)
            
        # Intentar reconocer con Google - necesita internet
        texto = reconocedor.recognize_google(audio, language='es-AR')
        print(f"Entendí: {texto}")
        return texto.lower()
        
    except sr.UnknownValueError:
        hablar("No pude entender lo que dijiste, probá de nuevo")
        return ""
    except sr.RequestError:
        hablar("Hay problema con internet, no me puedo conectar")
        return ""
    except sr.WaitTimeoutError:
        hablar("No escuché nada, se terminó el tiempo")
        return ""
    except Exception as error_general:
        print(f"Error raro con el micrófono: {error_general}")
        hablar("Algo salió mal con el micrófono")
        return ""

# -----------------------------------------
# FUNCIONES QUE FUI AGREGANDO CON EL TIEMPO
# -----------------------------------------

def buscar_en_internet(consulta):
    """
    Busca en Google lo que le digas
    """
    if not consulta:
        hablar("No me dijiste qué buscar")
        return
        
    print(f"Buscando en web: {consulta}")
    hablar(f"Buscando {consulta} en internet")
    
    # Limpiar un poco la consulta para la URL
    consulta_limpia = consulta.replace(' ', '+')
    webbrowser.open(f"https://www.google.com/search?q={consulta_limpia}")

def buscar_en_youtube(consulta):
    """
    Busca videos en YouTube
    """
    if not consulta:
        hablar("Decime qué querés buscar en YouTube")
        return
        
    print(f"Buscando en YouTube: {consulta}")
    hablar(f"Buscando {consulta} en YouTube")
    
    consulta_limpia = consulta.replace(' ', '+')
    webbrowser.open(f"https://www.youtube.com/results?search_query={consulta_limpia}")

def crear_archivo_texto():
    """
    Crea un archivo de texto con lo que le dictes
    Esto me costó hacerlo funcionar bien
    """
    hablar("¿Cómo querés llamar al archivo?")
    nombre_archivo = escuchar()
    
    if not nombre_archivo:
        hablar("No escuché el nombre del archivo")
        return
    
    # Agregar extensión si no la tiene
    if not nombre_archivo.endswith('.txt'):
        nombre_archivo += '.txt'
    
    hablar("Ahora decime el contenido del archivo")
    contenido = escuchar()
    
    if not contenido:
        hablar("No escuché el contenido")
        return
    
    try:
        # Intentar guardar el archivo
        with open(nombre_archivo, 'w', encoding='utf-8') as archivo:
            archivo.write(contenido)
        
        print(f"Archivo guardado: {nombre_archivo}")
        hablar(f"Listo, guardé el archivo {nombre_archivo.replace('.txt', '')}")
        
    except Exception as error:
        print(f"Error guardando archivo: {error}")
        hablar("No pude guardar el archivo, quizás el nombre no es válido")

# -----------------------------------------
# PROCESAR LO QUE LE DIGO - fui agregando comandos con el tiempo
# -----------------------------------------

def entender_comando(texto_entrada):
    """
    Intenta entender qué quiero que haga
    Fui agregando casos según se me ocurrian cosas
    """
    if not texto_entrada:
        return
    
    texto = texto_entrada.lower()
    print(f"Procesando comando: {texto}")
    
    # BUSCAR EN INTERNET
    if any(palabra in texto for palabra in ["buscar", "busca", "google"]):
        # Sacar las palabras de comando para obtener la búsqueda
        for palabra in ["buscar", "busca", "en google"]:
            texto = texto.replace(palabra, "")
        consulta = texto.strip()
        buscar_en_internet(consulta)
    
    # YOUTUBE
    elif any(palabra in texto for palabra in ["youtube", "video", "reproducir"]):
        for palabra in ["youtube", "video", "reproducir", "en youtube"]:
            texto = texto.replace(palabra, "")
        consulta = texto.strip()
        buscar_en_youtube(consulta)
    
    # ARCHIVOS DE TEXTO
    elif any(palabra in texto for palabra in ["escribir", "archivo", "texto", "anotar"]):
        crear_archivo_texto()
    
    # SALUDOS
    elif any(palabra in texto for palabra in ["hola", "hey", "buenas"]):
        hablar("¡Hola! ¿En qué te puedo ayudar?")
    
    # DESPEDIDAS
    elif any(palabra in texto for palabra in ["chau", "adiós", "nos vemos", "salir"]):
        hablar("¡Chau! Que tengas un buen día")
        return "salir"
    
    else:
        hablar("No entiendo ese comando. Podés pedirme que busque algo o cree un archivo")
        print(f"Comando no reconocido: {texto}")
    
    return "continuar"

# -----------------------------------------
# INTERFAZ GRÁFICA - esto lo agregué después, me costó aprender PySimpleGUI
# -----------------------------------------

def crear_interfaz():
    """
    Crea la ventana principal
    Fui probando diferentes temas hasta que me gustó este
    """
    # Probé varios temas: DarkBlue3, DarkGrey8, Topanga
    sg.theme("DarkBlue3")
    
    diseño = [
        [sg.Text("ADA - Mi Asistente de Voz", 
                font=("Segoe UI", 16, "bold"), 
                text_color="white")],
        
        [sg.HorizontalSeparator(color="lightblue")],
        
        [sg.Text("Escribí un comando o usá el micrófono:", 
                font=("Segoe UI", 10))],
        
        [sg.InputText(key="-ENTRADA-", 
                     size=(45, 1), 
                     font=("Segoe UI", 11),
                     tooltip="Ejemplo: buscar clima en Buenos Aires")],
        
        [sg.Button("Enviar", size=(8, 1), button_color=("white", "green")),
         sg.Button("Hablar", size=(8, 1), button_color=("white", "blue")),
         sg.Button("Salir", size=(8, 1), button_color=("white", "red"))],
         
        [sg.Text("Estado: Listo", 
                key="-ESTADO-", 
                font=("Segoe UI", 9), 
                text_color="lightgreen")]
    ]
    
    return sg.Window("ADA Asistente", 
                    diseño, 
                    finalize=True,
                    element_justification='center')

# -----------------------------------------
# PROGRAMA PRINCIPAL - acá es donde más cambié cosas
# -----------------------------------------

def main():
    """
    Función principal - la fui modificando según encontraba errores
    """
    print("Iniciando ADA...")
    print("Notas: Necesitás micrófono y conexión a internet")
    
    ventana = crear_interfaz()
    
    # Mensaje de bienvenida solo la primera vez
    hablar("Hola, soy ADA. Decime qué necesitás")
    
    while True:
        evento, valores = ventana.read(timeout=100)  # Timeout para no bloquear
        
        if evento in (sg.WINDOW_CLOSED, "Salir"):
            hablar("Hasta la próxima")
            break
            
        elif evento == "Enviar":
            texto_ingresado = valores["-ENTRADA-"].strip()
            if texto_ingresado:
                ventana["-ESTADO-"].update("Estado: Procesando...")
                resultado = entender_comando(texto_ingresado)
                ventana["-ENTRADA-"].update("")  # Limpiar entrada
                ventana["-ESTADO-"].update("Estado: Listo")
                
                if resultado == "salir":
                    break
                    
        elif evento == "Hablar":
            ventana["-ESTADO-"].update("Estado: Escuchando...")
            ventana.refresh()  # Forzar actualización
            
            comando_voz = escuchar()
            if comando_voz:
                ventana["-ENTRADA-"].update(comando_voz)
                resultado = entender_comando(comando_voz)
                ventana["-ESTADO-"].update("Estado: Listo")
                
                if resultado == "salir":
                    break
    
    ventana.close()
    print("ADA cerrado")

# Esto lo aprendí después - para que no se ejecute si importan el archivo
if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nPrograma interrumpido por el usuario")
    except Exception as error_grave:
        print(f"Error grave: {error_grave}")

        hablar("Tuve un error grave, necesito reiniciarme")
