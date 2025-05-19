import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.*;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.List;

@WebServlet("/memos")
public class getMemoServlet extends viewBaseServlet {
    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
// 设置编码和返回类型
        req.setCharacterEncoding("UTF-8");
        resp.setCharacterEncoding("UTF-8");
        resp.setContentType("text/plain;charset=UTF-8");
        resp.setHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        resp.setHeader("Pragma", "no-cache");
        resp.setHeader("Expires", "0");
        resp.setHeader("Access-Control-Allow-Origin", "http://212.129.223.4:8080");
        resp.setHeader("Access-Control-Allow-Credentials", "true");
        resp.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS");
        resp.setHeader("Access-Control-Allow-Headers", "Content-Type");


        HttpSession session = req.getSession();
        System.out.println("/memos");
        System.out.println("当前Session ID:"+session.getId());
        System.out.println("当前备忘录列表："+session.getAttribute("allContents"));
        System.out.println("请求Cookie:"+req.getHeader("Cookie"));
        System.out.println("*********************************************");

        List<String> allContents = (List<String>) session.getAttribute("allContents");

        PrintWriter out = resp.getWriter();
        if (allContents == null || allContents.isEmpty()) {
            out.println("暂无备忘录内容");
        } else {
            out.println("📋 当前备忘录内容：");
            for (String item : allContents) {
                out.println("- " + item);
            }
        }


//        super.processTemplate("index",req,resp);
    }
}
