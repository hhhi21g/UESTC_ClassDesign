import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.BufferedReader;
import java.io.IOException;
import java.net.URLDecoder;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

@WebServlet("/update")
public class UpdateServlet extends viewBaseServlet {


    @Override
    protected void doPut(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        req.setCharacterEncoding("UTF-8");
        resp.setContentType("text/html;charset=UTF-8");
        resp.setHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        resp.setHeader("Pragma", "no-cache");
        resp.setHeader("Expires", "0");
        resp.setHeader("Access-Control-Allow-Origin", "http://212.129.223.4:8080");
        resp.setHeader("Access-Control-Allow-Credentials", "true");
        resp.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
        resp.setHeader("Access-Control-Allow-Headers", "Content-Type");


        // 只读一次流
        String body = new BufferedReader(req.getReader()).lines().collect(Collectors.joining());
        String[] pairs = body.split("&");
        String oldNote = null, newNote = null;

        for (String pair : pairs) {
            String[] kv = pair.split("=");
            if (kv.length == 2) {
                if (kv[0].equals("old")) oldNote = URLDecoder.decode(kv[1], "UTF-8");
                if (kv[0].equals("note")) newNote = URLDecoder.decode(kv[1], "UTF-8");
            }
        }

        System.out.println("🔁 修改内容：old=" + oldNote + " → new=" + newNote);

        // 更新 session 中的内容
        HttpSession session = req.getSession();
        System.out.println("/update");
        System.out.println("当前Session ID:" + session.getId());
        System.out.println("当前备忘录列表：" + session.getAttribute("allContents"));
        System.out.println("请求Cookie:" + req.getHeader("Cookie"));
        System.out.println("*********************************************");


        List<String> allContents = (List<String>) session.getAttribute("allContents");
        if (allContents == null) {
            allContents = new ArrayList<>();
            session.setAttribute("allContents", allContents);
        }

        if (oldNote != null && newNote != null) {
            int index = allContents.indexOf(oldNote);
            if (index != -1) {
                allContents.set(index, newNote);
                session.setAttribute("allContents", allContents);
            }
        }


//        req.setAttribute("allContents", allContents);

        // 返回页面
//        super.processTemplate("index", req, resp);
//        resp.sendRedirect("http://212.129.223.4:8080/webTest/");
        // 修改只返回文本，不重定向
        resp.setContentType("text/plain;charset=UTF-8");
        resp.getWriter().write("ok");

    }


}
